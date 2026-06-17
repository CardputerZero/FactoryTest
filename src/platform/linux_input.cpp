#include "linux_input.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "audio_service.h"
#include "logger.h"

#if USE_DESKTOP
#include <SDL.h>
#endif

#if !USE_DESKTOP
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#endif

#ifndef APP_KEY_INPUT_DEVICE
#define APP_KEY_INPUT_DEVICE ""
#endif

namespace platform {
namespace {

constexpr uint32_t K_EVDEV_KEY_BASE   = 0x100000U;
constexpr uint32_t K_SPECIAL_KEY_BASE = 0x200000U;
constexpr uint32_t K_KEY_CTRL         = K_SPECIAL_KEY_BASE | 1U;
constexpr uint32_t K_KEY_SHIFT        = K_SPECIAL_KEY_BASE | 2U;
constexpr uint32_t K_KEY_SUPER        = K_SPECIAL_KEY_BASE | 3U;
constexpr uint32_t K_KEY_ALT          = K_SPECIAL_KEY_BASE | 4U;
constexpr uint32_t K_KEY_FN           = K_SPECIAL_KEY_BASE | 5U;
constexpr uint32_t K_KEY_SYM          = K_SPECIAL_KEY_BASE | 6U;
constexpr uint32_t K_KEY_INSERT       = K_SPECIAL_KEY_BASE | 7U;
constexpr uint32_t K_KEY_PAGE_UP      = K_SPECIAL_KEY_BASE | 8U;
constexpr uint32_t K_KEY_PAGE_DOWN    = K_SPECIAL_KEY_BASE | 9U;
constexpr uint32_t K_KEY_F_BASE       = K_SPECIAL_KEY_BASE | 0x100U;
constexpr uint32_t K_LONG_PRESS_MS    = 700;

struct KeyRouterState {
  std::array<lv_obj_t*, K_NAV_KEY_COUNT> nav_buttons{};
  uint32_t last_key{0};
  bool last_key_pressed{false};
  uint32_t pressed_key{0};
  uint32_t press_started_at{0};
  uint32_t last_key_activity_at{0};
  bool long_press_sent{false};
  lv_timer_t* long_press_timer{nullptr};
  NavTriggerMode nav_trigger_mode{NavTriggerMode::CLICK};
  KeyListener key_listener{nullptr};
  void* key_listener_user_data{nullptr};
  GlobalKeyListener global_key_listener{nullptr};
  void* global_key_listener_user_data{nullptr};
};

#if !USE_DESKTOP
struct EvdevKeypad {
  int fd{-1};
  lv_indev_state_t state{LV_INDEV_STATE_RELEASED};
  uint32_t key{0};
  bool shift_pressed{false};
  bool synthetic_release_pending{false};
  bool router_event_pending{false};
  uint32_t router_key{0};
  bool router_pressed{false};
};
#endif

KeyRouterState& key_router_state() {
  static KeyRouterState state;
  return state;
}

const char* event_code_name(lv_event_code_t event_code) {
  switch (event_code) {
    case LV_EVENT_PRESSED:
      return "PRESSED";
    case LV_EVENT_PRESSING:
      return "PRESSING";
    case LV_EVENT_RELEASED:
      return "RELEASED";
    case LV_EVENT_CLICKED:
      return "CLICKED";
    case LV_EVENT_LONG_PRESSED:
      return "LONG_PRESSED";
    case LV_EVENT_LONG_PRESSED_REPEAT:
      return "LONG_PRESSED_REPEAT";
    case LV_EVENT_KEY:
      return "KEY";
    default:
      return "OTHER";
  }
}

const char* key_state_name(bool pressed) { return pressed ? "pressed" : "released"; }

std::string key_name(uint32_t key) { return describe_key(key); }

size_t nav_key_to_index(uint32_t key) {
  switch (key) {
    case '4':
    case LV_KEY_ESC:
      return 0;
    case '5':
    case 'z':
    case 'Z':
    case LV_KEY_LEFT:
      return 1;
    case '6':
      return 2;
    case '7':
      return 3;
    case '8':
      return 4;
    case 'c':
    case 'C':
    case LV_KEY_RIGHT:
      return key_router_state().nav_trigger_mode == NavTriggerMode::CLICK ? 3 : 4;
    case LV_KEY_ENTER:
      return 2;
    default:
      return K_NAV_KEY_COUNT;
  }
}

void dispatch_nav_key(uint32_t key, lv_event_code_t event_code) {
  auto& state      = key_router_state();
  const auto index = nav_key_to_index(key);
  if (index >= state.nav_buttons.size()) {
    LOG_DEBUG("key router: no nav mapping key={} event={}",
              key_name(key),
              event_code_name(event_code));
    return;
  }

  auto* button = state.nav_buttons[index];
  if (!button || !lv_obj_is_valid(button) || !lv_obj_has_flag(button, LV_OBJ_FLAG_CLICKABLE)) {
    LOG_DEBUG("key router: nav target unavailable key={} index={} event={}",
              key_name(key),
              index,
              event_code_name(event_code));
    return;
  }

  LOG_DEBUG("key router: dispatch key={} index={} event={}",
            key_name(key),
            index,
            event_code_name(event_code));
  lv_obj_send_event(button, event_code, nullptr);
}

void emit_key(uint32_t key) {
  auto& state     = key_router_state();
  const auto name = key_name(key);
  if (state.global_key_listener) {
    state.global_key_listener(key, name.c_str(), false, state.global_key_listener_user_data);
  }
  if (state.key_listener) {
    state.key_listener(key, name.c_str(), state.key_listener_user_data);
  }
}

void emit_long_key(uint32_t key) {
  auto& state     = key_router_state();
  const auto name = key_name(key);
  if (state.global_key_listener) {
    state.global_key_listener(key, name.c_str(), true, state.global_key_listener_user_data);
  }
}

void long_press_timer_cb(lv_timer_t* timer) {
  LV_UNUSED(timer);

  auto& state = key_router_state();
  if (!state.pressed_key || state.long_press_sent) {
    return;
  }

  if (lv_tick_elaps(state.press_started_at) < K_LONG_PRESS_MS) {
    return;
  }

  state.long_press_sent = true;
  LOG_DEBUG("key router: long press key={} elapsed={}ms",
            key_name(state.pressed_key),
            lv_tick_elaps(state.press_started_at));
  emit_long_key(state.pressed_key);
  dispatch_nav_key(state.pressed_key, LV_EVENT_LONG_PRESSED);
}

void ensure_long_press_timer() {
  auto& state = key_router_state();
  if (!state.long_press_timer) {
    state.long_press_timer = lv_timer_create(long_press_timer_cb, 50, nullptr);
  }
}

void route_key_state(uint32_t key, bool pressed, const char* source) {
  auto& state            = key_router_state();
  const bool is_new_hold = !state.last_key_pressed || key != state.pressed_key;
  LOG_DEBUG("key router: input source={} key={} state={} last_key={} last_pressed={} mode={}",
            source ? source : "unknown",
            key_name(key),
            key_state_name(pressed),
            key_name(state.last_key),
            state.last_key_pressed,
            state.nav_trigger_mode == NavTriggerMode::CLICK ? "click" : "long");
  if (pressed) {
    if (is_new_hold) {
      state.pressed_key      = key;
      state.press_started_at = lv_tick_get();
      state.long_press_sent  = false;
      audio::play_key_click_sound();
      emit_key(key);
      dispatch_nav_key(key, LV_EVENT_PRESSED);
    }
    state.last_key_activity_at = lv_tick_get();
    ensure_long_press_timer();
    if (state.nav_trigger_mode == NavTriggerMode::CLICK &&
        (!state.last_key_pressed || state.last_key != key)) {
      dispatch_nav_key(key, LV_EVENT_CLICKED);
    }
  } else if (state.last_key_pressed && state.pressed_key == key) {
    dispatch_nav_key(key, LV_EVENT_RELEASED);
    state.pressed_key = 0;
  } else {
    LOG_DEBUG("key router: ignored release key={} pressed_key={} last_pressed={}",
              key_name(key),
              key_name(state.pressed_key),
              state.last_key_pressed);
  }

  state.last_key         = key;
  state.last_key_pressed = pressed;
}

void key_event_cb(lv_event_t* event) {
  LV_UNUSED(event);

  auto* indev = lv_indev_active();
  if (!indev) {
    return;
  }

#if !USE_DESKTOP
  auto* keypad = static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
  if (keypad) {
    if (!keypad->router_event_pending) {
      return;
    }

    keypad->router_event_pending = false;
    route_key_state(keypad->router_key, keypad->router_pressed, "evdev");
    return;
  }
#endif

  const auto key     = lv_indev_get_key(indev);
  const bool pressed = lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED;
  route_key_state(key, pressed, "lvgl");
}

#if USE_DESKTOP
uint32_t map_sdl_modifier_key(SDL_Keycode key) {
  switch (key) {
    case SDLK_LCTRL:
    case SDLK_RCTRL:
      return K_KEY_CTRL;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
      return K_KEY_SHIFT;
    case SDLK_LGUI:
    case SDLK_RGUI:
      return K_KEY_SUPER;
    case SDLK_LALT:
    case SDLK_RALT:
      return K_KEY_ALT;
    default:
      return 0;
  }
}

int sdl_modifier_event_watch(void* user_data, SDL_Event* event) {
  LV_UNUSED(user_data);

  if (!event || event->type != SDL_KEYDOWN) {
    return 1;
  }

  const auto key = map_sdl_modifier_key(event->key.keysym.sym);
  if (key) {
    emit_key(key);
  }
  return 1;
}
#endif

const char* named_lv_key(uint32_t key) {
  switch (key) {
    case LV_KEY_UP:
      return "Up";
    case LV_KEY_DOWN:
      return "Down";
    case LV_KEY_RIGHT:
      return "Right";
    case LV_KEY_LEFT:
      return "Left";
    case LV_KEY_ESC:
      return "Esc";
    case LV_KEY_DEL:
      return "Delete";
    case LV_KEY_BACKSPACE:
      return "Backspace";
    case LV_KEY_ENTER:
      return "Enter";
    case LV_KEY_NEXT:
      return "Next";
    case LV_KEY_PREV:
      return "Previous";
    case LV_KEY_HOME:
      return "Home";
    case LV_KEY_END:
      return "End";
    default:
      return nullptr;
  }
}

#if !USE_DESKTOP
uint32_t shifted_number_key(uint16_t code) {
  switch (code) {
    case KEY_1:
      return '!';
    case KEY_2:
      return '@';
    case KEY_3:
      return '#';
    case KEY_4:
      return '$';
    case KEY_5:
      return '%';
    case KEY_6:
      return '^';
    case KEY_7:
      return '&';
    case KEY_8:
      return '*';
    case KEY_9:
      return '(';
    case KEY_0:
      return ')';
    default:
      return 0;
  }
}

uint32_t printable_evdev_key(uint16_t code, bool shift_pressed) {
  if (code >= KEY_1 && code <= KEY_9) {
    if (shift_pressed) {
      return shifted_number_key(code);
    }
    return static_cast<uint32_t>('1' + (code - KEY_1));
  }
  if (code == KEY_0) {
    return shift_pressed ? ')' : '0';
  }
  if (code >= KEY_Q && code <= KEY_P) {
    return static_cast<uint32_t>((shift_pressed ? "QWERTYUIOP" : "qwertyuiop")[code - KEY_Q]);
  }
  if (code >= KEY_A && code <= KEY_L) {
    return static_cast<uint32_t>((shift_pressed ? "ASDFGHJKL" : "asdfghjkl")[code - KEY_A]);
  }
  if (code >= KEY_Z && code <= KEY_M) {
    return static_cast<uint32_t>((shift_pressed ? "ZXCVBNM" : "zxcvbnm")[code - KEY_Z]);
  }

  switch (code) {
    case KEY_SPACE:
      return ' ';
    case KEY_MINUS:
      return shift_pressed ? '_' : '-';
    case KEY_EQUAL:
      return shift_pressed ? '+' : '=';
    case KEY_LEFTBRACE:
      return shift_pressed ? '{' : '[';
    case KEY_RIGHTBRACE:
      return shift_pressed ? '}' : ']';
    case KEY_BACKSLASH:
      return shift_pressed ? '|' : '\\';
    case KEY_SEMICOLON:
      return shift_pressed ? ':' : ';';
    case KEY_APOSTROPHE:
      return shift_pressed ? '"' : '\'';
    case KEY_GRAVE:
      return shift_pressed ? '~' : '`';
    case KEY_COMMA:
      return shift_pressed ? '<' : ',';
    case KEY_DOT:
      return shift_pressed ? '>' : '.';
    case KEY_SLASH:
      return shift_pressed ? '?' : '/';
    case KEY_KPASTERISK:
      return '*';
    case KEY_KPMINUS:
      return '-';
    case KEY_KPPLUS:
      return '+';
    case KEY_KPDOT:
      return '.';
    case KEY_KPSLASH:
      return '/';
    default:
      return 0;
  }
}

uint32_t map_evdev_key(uint16_t code, bool shift_pressed) {
  const auto printable = printable_evdev_key(code, shift_pressed);
  if (printable) {
    return printable;
  }

  switch (code) {
    case KEY_ESC:
      return LV_KEY_ESC;
    case KEY_ENTER:
      return LV_KEY_ENTER;
    case KEY_BACKSPACE:
      return LV_KEY_BACKSPACE;
    case KEY_DELETE:
      return LV_KEY_DEL;
    case KEY_INSERT:
      return K_KEY_INSERT;
    case KEY_UP:
      return LV_KEY_UP;
    case KEY_DOWN:
      return LV_KEY_DOWN;
    case KEY_LEFT:
      return LV_KEY_LEFT;
    case KEY_RIGHT:
      return LV_KEY_RIGHT;
    case KEY_HOME:
      return LV_KEY_HOME;
    case KEY_END:
      return LV_KEY_END;
    case KEY_PAGEUP:
      return K_KEY_PAGE_UP;
    case KEY_PAGEDOWN:
      return K_KEY_PAGE_DOWN;
    case KEY_TAB:
      return '\t';
    case KEY_F1:
    case KEY_F2:
    case KEY_F3:
    case KEY_F4:
    case KEY_F5:
    case KEY_F6:
    case KEY_F7:
    case KEY_F8:
    case KEY_F9:
    case KEY_F10:
      return K_KEY_F_BASE | static_cast<uint32_t>(code - KEY_F1 + 1);
    case KEY_F11:
    case KEY_F12:
      return K_KEY_F_BASE | static_cast<uint32_t>(code - KEY_F11 + 11);
    case KEY_FN:
      return K_KEY_FN;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
      return K_KEY_CTRL;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
      return K_KEY_SHIFT;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
      return K_KEY_SUPER;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
      return K_KEY_ALT;
    default:
      return K_EVDEV_KEY_BASE | code;
  }
}

uint32_t map_evdev_scan(uint32_t scan_code) {
  switch (scan_code) {
    case 66:
      return K_KEY_FN;
    case 67:
      return K_KEY_SYM;
    default:
      return 0;
  }
}

void queue_router_event(EvdevKeypad* keypad, uint32_t key, bool pressed, const char* source) {
  if (!keypad) {
    return;
  }

  keypad->router_event_pending = true;
  keypad->router_key           = key;
  keypad->router_pressed       = pressed;
  LOG_DEBUG("evdev: queue router source={} key={} state={}",
            source ? source : "unknown",
            key_name(key),
            key_state_name(pressed));
}

bool has_keyboard_keys(int fd) {
  unsigned long key_bits[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
    return false;
  }

  auto has_key = [&](int code) {
    const auto bits_per_word = static_cast<int>(sizeof(unsigned long) * 8);
    return (key_bits[code / bits_per_word] & (1UL << (code % bits_per_word))) != 0;
  };

  return has_key(KEY_A) || has_key(KEY_ENTER) || has_key(KEY_ESC) || has_key(KEY_SPACE) ||
         has_key(KEY_4) || has_key(KEY_5) || has_key(KEY_6) || has_key(KEY_7) || has_key(KEY_8);
}

void evdev_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
  auto* keypad           = static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
  data->continue_reading = false;
  if (!keypad) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  data->key   = keypad->key;
  data->state = keypad->state;

  if (keypad->synthetic_release_pending) {
    keypad->synthetic_release_pending = false;
    keypad->state                     = LV_INDEV_STATE_RELEASED;
    data->key                         = keypad->key;
    data->state                       = keypad->state;
    data->continue_reading            = true;
    queue_router_event(keypad, keypad->key, false, "msc-synthetic-release");
    return;
  }

  input_event input{};
  while (read(keypad->fd, &input, sizeof(input)) == sizeof(input)) {
    LOG_DEBUG("evdev: raw type={} code={} value={}", input.type, input.code, input.value);
    if (input.type == EV_MSC && input.code == MSC_SCAN) {
      const auto scan_key = map_evdev_scan(static_cast<uint32_t>(input.value));
      if (!scan_key) {
        LOG_DEBUG("evdev: ignored MSC_SCAN value={}", input.value);
        continue;
      }

      keypad->key                       = scan_key;
      keypad->state                     = LV_INDEV_STATE_PRESSED;
      keypad->synthetic_release_pending = true;
      data->continue_reading            = true;
      queue_router_event(keypad, keypad->key, true, "msc-synthetic-press");
      break;
    }

    if (input.type != EV_KEY) {
      continue;
    }

    if (input.value == 2) {
      LOG_DEBUG("evdev: ignored repeat code={}", input.code);
      continue;
    }

    if (input.code == KEY_LEFTSHIFT || input.code == KEY_RIGHTSHIFT) {
      keypad->shift_pressed = input.value != 0;
    }

    keypad->key            = map_evdev_key(input.code, keypad->shift_pressed);
    keypad->state          = input.value ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading = true;
    queue_router_event(keypad, keypad->key, keypad->state == LV_INDEV_STATE_PRESSED, "ev-key");
    break;
  }

  data->key   = keypad->key;
  data->state = keypad->state;
}

void evdev_delete_cb(lv_event_t* event) {
  auto* indev  = static_cast<lv_indev_t*>(lv_event_get_target(event));
  auto* keypad = static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
  if (!keypad) {
    return;
  }

  if (keypad->fd >= 0) {
    close(keypad->fd);
  }
  delete keypad;
}

lv_indev_t* create_keypad_from_fd(int fd) {
  auto* keypad = new EvdevKeypad;
  keypad->fd   = fd;

  auto* indev = lv_indev_create();
  if (!indev) {
    LOG_ERROR("failed to create LVGL keypad input device for fd {}", fd);
    delete keypad;
    close(fd);
    return nullptr;
  }

  lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(indev, evdev_read_cb);
  lv_indev_set_driver_data(indev, keypad);
  lv_indev_add_event_cb(indev, evdev_delete_cb, LV_EVENT_DELETE, nullptr);
  attach_key_router(indev);
  return indev;
}

lv_indev_t* try_create_keypad(const char* path) {
  if (!path || path[0] == '\0') {
    return nullptr;
  }

  LOG_DEBUG("probing evdev input device: {}", path);
  const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    LOG_WARN("failed to open input device {}: {}", path, strerror(errno));
    LV_LOG_WARN("failed to open input device %s: %s", path, strerror(errno));
    return nullptr;
  }

  if (!has_keyboard_keys(fd)) {
    LOG_DEBUG("input device does not expose expected keyboard keys: {}", path);
    close(fd);
    return nullptr;
  }

  LOG_INFO("using evdev key input device: {}", path);
  LV_LOG_INFO("using evdev key input %s", path);
  return create_keypad_from_fd(fd);
}

void discover_keypads(lv_display_t* display) {
  const char* configured_device = APP_KEY_INPUT_DEVICE;
  if (configured_device[0] != '\0') {
    LOG_DEBUG("initializing configured key input device: {}", configured_device);
    auto* indev = try_create_keypad(configured_device);
    if (indev) {
      lv_indev_set_display(indev, display);
      LOG_INFO("configured key input initialized successfully: {}", configured_device);
    } else {
      LOG_ERROR("configured key input initialization failed: {}", configured_device);
    }
    return;
  }

  LOG_DEBUG("discovering key input devices under /dev/input");
  auto* dir = opendir("/dev/input");
  if (!dir) {
    LOG_WARN("failed to open /dev/input: {}", strerror(errno));
    LV_LOG_WARN("failed to open /dev/input: %s", strerror(errno));
    return;
  }

  int initialized_count = 0;
  while (auto* entry = readdir(dir)) {
    if (std::strncmp(entry->d_name, "event", 5) != 0) {
      continue;
    }

    std::string path = "/dev/input/";
    path += entry->d_name;
    auto* indev = try_create_keypad(path.c_str());
    if (indev) {
      lv_indev_set_display(indev, display);
      ++initialized_count;
    }
  }

  closedir(dir);
  LOG_INFO("key input discovery completed: {} device(s) initialized", initialized_count);
}
#endif

}  // namespace

void init_key_input(lv_display_t* display) {
#if !USE_DESKTOP
  LOG_DEBUG("initializing Linux evdev key input");
  discover_keypads(display);
#else
  LV_UNUSED(display);
  LOG_DEBUG("desktop key input initialized through SDL/LVGL");
#endif
}

void attach_key_router(lv_indev_t* indev) {
  if (!indev || lv_indev_get_type(indev) != LV_INDEV_TYPE_KEYPAD) {
    LOG_WARN("skipping key router attach for invalid or non-keypad input device");
    return;
  }

  lv_indev_add_event_cb(indev, key_event_cb, LV_EVENT_KEY, nullptr);
  LOG_DEBUG("attached key router to keypad input device");
#if USE_DESKTOP
  static bool sdl_event_watch_added = false;
  if (!sdl_event_watch_added) {
    SDL_AddEventWatch(sdl_modifier_event_watch, nullptr);
    sdl_event_watch_added = true;
    LOG_DEBUG("registered SDL modifier key event watcher");
  }
#endif
}

void register_nav_button(size_t index, lv_obj_t* button) {
  auto& state = key_router_state();
  if (index >= state.nav_buttons.size()) {
    return;
  }

  state.nav_buttons[index] = button;
}

void unregister_nav_button(size_t index, lv_obj_t* button) {
  auto& state = key_router_state();
  if (index >= state.nav_buttons.size()) {
    return;
  }

  if (!button || state.nav_buttons[index] == button) {
    state.nav_buttons[index] = nullptr;
  }
}

void set_key_listener(KeyListener listener, void* user_data) {
  auto& state                  = key_router_state();
  state.key_listener           = listener;
  state.key_listener_user_data = user_data;
}

void clear_key_listener(KeyListener listener, void* user_data) {
  auto& state = key_router_state();
  if (state.key_listener == listener && state.key_listener_user_data == user_data) {
    state.key_listener           = nullptr;
    state.key_listener_user_data = nullptr;
  }
}

void set_global_key_listener(GlobalKeyListener listener, void* user_data) {
  auto& state                         = key_router_state();
  state.global_key_listener           = listener;
  state.global_key_listener_user_data = user_data;
}

void clear_global_key_listener(GlobalKeyListener listener, void* user_data) {
  auto& state = key_router_state();
  if (state.global_key_listener == listener && state.global_key_listener_user_data == user_data) {
    state.global_key_listener           = nullptr;
    state.global_key_listener_user_data = nullptr;
  }
}

void set_nav_trigger_mode(NavTriggerMode mode) {
  auto& state = key_router_state();
  LOG_DEBUG("key router: trigger mode {} -> {}",
            state.nav_trigger_mode == NavTriggerMode::CLICK ? "click" : "long",
            mode == NavTriggerMode::CLICK ? "click" : "long");
  state.nav_trigger_mode     = mode;
  state.pressed_key          = 0;
  state.last_key_pressed     = false;
  state.last_key_activity_at = 0;
  state.long_press_sent      = false;
}

const char* describe_key(uint32_t key) {
  static char buffer[32];

  if (const char* name = named_lv_key(key)) {
    return name;
  }
  if (key == ' ') {
    return "Space";
  }
  if (key == '\t') {
    return "Tab";
  }
  if (key >= 32 && key <= 126) {
    std::snprintf(buffer, sizeof(buffer), "%c", static_cast<char>(key));
    return buffer;
  }
  if (key == K_KEY_CTRL) {
    return "Ctrl";
  }
  if (key == K_KEY_SHIFT) {
    return "Shift";
  }
  if (key == K_KEY_SUPER) {
    return "Super";
  }
  if (key == K_KEY_ALT) {
    return "Alt";
  }
  if (key == K_KEY_FN) {
    return "Fn";
  }
  if (key == K_KEY_SYM) {
    return "Sym";
  }
  if (key == K_KEY_INSERT) {
    return "Insert";
  }
  if (key == K_KEY_PAGE_UP) {
    return "PageUp";
  }
  if (key == K_KEY_PAGE_DOWN) {
    return "PageDown";
  }
  if ((key & K_KEY_F_BASE) == K_KEY_F_BASE) {
    std::snprintf(buffer, sizeof(buffer), "F%u", key & ~K_KEY_F_BASE);
    return buffer;
  }
  if ((key & K_EVDEV_KEY_BASE) == K_EVDEV_KEY_BASE) {
    std::snprintf(buffer, sizeof(buffer), "KEY_%u", key & ~K_EVDEV_KEY_BASE);
    return buffer;
  }

  std::snprintf(buffer, sizeof(buffer), "Key %u", key);
  return buffer;
}

}  // namespace platform
