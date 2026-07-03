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

constexpr uint32_t K_EVDEV_KEY_BASE      = 0x100000U;
constexpr uint32_t K_SPECIAL_KEY_BASE    = 0x200000U;
constexpr uint32_t K_KEY_CTRL            = K_SPECIAL_KEY_BASE | 1U;
constexpr uint32_t K_KEY_SHIFT           = K_SPECIAL_KEY_BASE | 2U;
constexpr uint32_t K_KEY_SUPER           = K_SPECIAL_KEY_BASE | 3U;
constexpr uint32_t K_KEY_ALT             = K_SPECIAL_KEY_BASE | 4U;
constexpr uint32_t K_KEY_FN              = K_SPECIAL_KEY_BASE | 5U;
constexpr uint32_t K_KEY_SYM             = K_SPECIAL_KEY_BASE | 6U;
constexpr uint32_t K_KEY_INSERT          = K_SPECIAL_KEY_BASE | 7U;
constexpr uint32_t K_KEY_PAGE_UP         = K_SPECIAL_KEY_BASE | 8U;
constexpr uint32_t K_KEY_PAGE_DOWN       = K_SPECIAL_KEY_BASE | 9U;
constexpr uint32_t K_KEY_VOLUME_MUTE     = K_SPECIAL_KEY_BASE | 10U;
constexpr uint32_t K_KEY_VOLUME_DOWN     = K_SPECIAL_KEY_BASE | 11U;
constexpr uint32_t K_KEY_VOLUME_UP       = K_SPECIAL_KEY_BASE | 12U;
constexpr uint32_t K_KEY_BRIGHTNESS_DOWN = K_SPECIAL_KEY_BASE | 13U;
constexpr uint32_t K_KEY_BRIGHTNESS_UP   = K_SPECIAL_KEY_BASE | 14U;
constexpr uint32_t K_KEY_F_BASE          = K_SPECIAL_KEY_BASE | 0x100U;
constexpr uint32_t K_LONG_PRESS_MS       = 700;

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
  KeyReleaseListener key_release_listener{nullptr};
  void* key_release_listener_user_data{nullptr};
  LongKeyListener long_key_listener{nullptr};
  void* long_key_listener_user_data{nullptr};
  GlobalKeyListener global_key_listener{nullptr};
  void* global_key_listener_user_data{nullptr};
};

struct KeyMapEntry {
  uint32_t source;
  uint32_t key;
};

struct KeyNameEntry {
  uint32_t key;
  const char* name;
};

struct EventCodeNameEntry {
  lv_event_code_t code;
  const char* name;
};

struct NavKeyMapEntry {
  uint32_t key;
  size_t click_index;
  size_t long_press_index;
};

constexpr std::array<EventCodeNameEntry, 7> K_EVENT_CODE_NAMES = {{
    {LV_EVENT_PRESSED, "PRESSED"},
    {LV_EVENT_PRESSING, "PRESSING"},
    {LV_EVENT_RELEASED, "RELEASED"},
    {LV_EVENT_CLICKED, "CLICKED"},
    {LV_EVENT_LONG_PRESSED, "LONG_PRESSED"},
    {LV_EVENT_LONG_PRESSED_REPEAT, "LONG_PRESSED_REPEAT"},
    {LV_EVENT_KEY, "KEY"},
}};

constexpr std::array<NavKeyMapEntry, 12> K_NAV_KEY_MAP = {{
    {'4', 0, 0},
    {LV_KEY_ESC, 0, 0},
    {'5', 1, 1},
    {'z', 1, 1},
    {'Z', 1, 1},
    {LV_KEY_LEFT, 1, 1},
    {'6', 2, 2},
    {'7', 3, 3},
    {'8', 4, 4},
    {'c', 3, 3},
    {'C', 3, 3},
    {LV_KEY_RIGHT, 3, 3},
}};

constexpr std::array<KeyNameEntry, 12> K_LV_KEY_NAMES = {{
    {LV_KEY_UP, "Up"},
    {LV_KEY_DOWN, "Down"},
    {LV_KEY_RIGHT, "Right"},
    {LV_KEY_LEFT, "Left"},
    {LV_KEY_ESC, "Esc"},
    {LV_KEY_DEL, "Delete"},
    {LV_KEY_BACKSPACE, "Backspace"},
    {LV_KEY_ENTER, "Enter"},
    {LV_KEY_NEXT, "Next"},
    {LV_KEY_PREV, "Previous"},
    {LV_KEY_HOME, "Home"},
    {LV_KEY_END, "End"},
}};

constexpr std::array<KeyNameEntry, 16> K_SPECIAL_KEY_NAMES = {{
    {' ', "Space"},
    {'\t', "Tab"},
    {K_KEY_CTRL, "Ctrl"},
    {K_KEY_SHIFT, "Shift"},
    {K_KEY_SUPER, "Super"},
    {K_KEY_ALT, "Alt"},
    {K_KEY_FN, "Fn"},
    {K_KEY_SYM, "Sym"},
    {K_KEY_INSERT, "Insert"},
    {K_KEY_PAGE_UP, "PageUp"},
    {K_KEY_PAGE_DOWN, "PageDown"},
    {K_KEY_VOLUME_MUTE, "VolumeMute"},
    {K_KEY_VOLUME_DOWN, "VolumeDown"},
    {K_KEY_VOLUME_UP, "VolumeUp"},
    {K_KEY_BRIGHTNESS_DOWN, "BrightnessDown"},
    {K_KEY_BRIGHTNESS_UP, "BrightnessUp"},
}};

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

template <std::size_t N>
uint32_t lookup_key(const std::array<KeyMapEntry, N>& entries, uint32_t source) {
  for (const auto& entry : entries) {
    if (entry.source == source) {
      return entry.key;
    }
  }
  return 0;
}

template <std::size_t N>
const char* lookup_name(const std::array<KeyNameEntry, N>& entries, uint32_t key) {
  for (const auto& entry : entries) {
    if (entry.key == key) {
      return entry.name;
    }
  }
  return nullptr;
}

const char* event_code_name(lv_event_code_t event_code) {
  for (const auto& entry : K_EVENT_CODE_NAMES) {
    if (entry.code == event_code) {
      return entry.name;
    }
  }
  return "OTHER";
}

const char* key_state_name(bool pressed) { return pressed ? "pressed" : "released"; }

std::string key_name(uint32_t key) { return describe_key(key); }

size_t nav_key_to_index(uint32_t key) {
  for (const auto& entry : K_NAV_KEY_MAP) {
    if (entry.key == key) {
      return key_router_state().nav_trigger_mode == NavTriggerMode::CLICK ? entry.click_index
                                                                          : entry.long_press_index;
    }
  }
  return K_NAV_KEY_COUNT;
}

void dispatch_nav_key(uint32_t key, lv_event_code_t event_code) {
  auto& state      = key_router_state();
  const auto index = nav_key_to_index(key);
  if (index >= state.nav_buttons.size()) {
    LOG_VERBOSE("key router: no nav mapping key={} event={}",
                key_name(key),
                event_code_name(event_code));
    return;
  }

  auto* button = state.nav_buttons[index];
  if (!button || !lv_obj_is_valid(button) || !lv_obj_has_flag(button, LV_OBJ_FLAG_CLICKABLE)) {
    LOG_VERBOSE("key router: nav target unavailable key={} index={} event={}",
                key_name(key),
                index,
                event_code_name(event_code));
    return;
  }

  LOG_VERBOSE("key router: dispatch key={} index={} event={}",
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

void emit_key_release(uint32_t key) {
  auto& state     = key_router_state();
  const auto name = key_name(key);
  if (state.key_release_listener) {
    state.key_release_listener(key, name.c_str(), state.key_release_listener_user_data);
  }
}

void emit_long_key(uint32_t key) {
  auto& state     = key_router_state();
  const auto name = key_name(key);
  if (state.global_key_listener) {
    state.global_key_listener(key, name.c_str(), true, state.global_key_listener_user_data);
  }
  if (state.long_key_listener) {
    state.long_key_listener(key, name.c_str(), state.long_key_listener_user_data);
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
  LOG_VERBOSE("key router: long press key={} elapsed={}ms",
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
  LOG_VERBOSE("key router: input source={} key={} state={} last_key={} last_pressed={} mode={}",
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
    emit_key_release(key);
    state.pressed_key = 0;
  } else {
    LOG_VERBOSE("key router: ignored release key={} pressed_key={} last_pressed={}",
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
constexpr std::array<KeyMapEntry, 8> K_SDL_MODIFIER_KEYS = {{
    {SDLK_LCTRL, K_KEY_CTRL},
    {SDLK_RCTRL, K_KEY_CTRL},
    {SDLK_LSHIFT, K_KEY_SHIFT},
    {SDLK_RSHIFT, K_KEY_SHIFT},
    {SDLK_LGUI, K_KEY_SUPER},
    {SDLK_RGUI, K_KEY_SUPER},
    {SDLK_LALT, K_KEY_ALT},
    {SDLK_RALT, K_KEY_ALT},
}};

uint32_t map_sdl_modifier_key(SDL_Keycode key) {
  return lookup_key(K_SDL_MODIFIER_KEYS, static_cast<uint32_t>(key));
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

const char* named_lv_key(uint32_t key) { return lookup_name(K_LV_KEY_NAMES, key); }

#if !USE_DESKTOP
struct EvdevPrintableKeyMapEntry {
  uint16_t code;
  uint32_t key;
  uint32_t shifted_key;
};

struct EvdevAlphaKeyRange {
  uint16_t first_code;
  uint16_t last_code;
  const char* keys;
  const char* shifted_keys;
};

constexpr std::array<KeyMapEntry, 32> K_TCA8418_SYMBOL_KEYS = {{
    {183, '!'}, {184, '@'},  {185, '#'},  {186, '$'}, {187, '%'}, {188, '^'}, {189, '&'},
    {190, '*'}, {191, '('},  {192, ')'},  {193, '~'}, {194, '`'}, {195, '+'}, {196, '-'},
    {197, '/'}, {198, '\\'}, {199, '{'},  {200, '}'}, {201, '['}, {202, ']'}, {231, ','},
    {232, '.'}, {233, '|'},  {209, '='},  {210, ':'}, {211, ';'}, {212, '_'}, {213, '?'},
    {214, '<'}, {215, '>'},  {216, '\''}, {217, '"'},
}};

constexpr std::array<EvdevPrintableKeyMapEntry, 22> K_PRINTABLE_SYMBOL_KEYS = {{
    {KEY_1, '1', '!'},          {KEY_2, '2', '@'},         {KEY_3, '3', '#'},
    {KEY_4, '4', '$'},          {KEY_5, '5', '%'},         {KEY_6, '6', '^'},
    {KEY_7, '7', '&'},          {KEY_8, '8', '*'},         {KEY_9, '9', '('},
    {KEY_0, '0', ')'},          {KEY_SPACE, ' ', ' '},     {KEY_MINUS, '-', '_'},
    {KEY_EQUAL, '=', '+'},      {KEY_LEFTBRACE, '[', '{'}, {KEY_RIGHTBRACE, ']', '}'},
    {KEY_BACKSLASH, '\\', '|'}, {KEY_SEMICOLON, ';', ':'}, {KEY_APOSTROPHE, '\'', '"'},
    {KEY_GRAVE, '`', '~'},      {KEY_COMMA, ',', '<'},     {KEY_DOT, '.', '>'},
    {KEY_SLASH, '/', '?'},
}};

constexpr std::array<KeyMapEntry, 5> K_KEYPAD_SYMBOL_KEYS = {{
    {KEY_KPASTERISK, '*'},
    {KEY_KPMINUS, '-'},
    {KEY_KPPLUS, '+'},
    {KEY_KPDOT, '.'},
    {KEY_KPSLASH, '/'},
}};

constexpr std::array<KeyMapEntry, 29> K_SPECIAL_EVDEV_KEYS = {{
    {KEY_ESC, LV_KEY_ESC},
    {KEY_ENTER, LV_KEY_ENTER},
    {KEY_KPENTER, LV_KEY_ENTER},
    {KEY_BACKSPACE, LV_KEY_BACKSPACE},
    {KEY_DELETE, LV_KEY_DEL},
    {KEY_INSERT, K_KEY_INSERT},
    {KEY_UP, LV_KEY_UP},
    {KEY_DOWN, LV_KEY_DOWN},
    {KEY_LEFT, LV_KEY_LEFT},
    {KEY_RIGHT, LV_KEY_RIGHT},
    {KEY_HOME, LV_KEY_HOME},
    {KEY_END, LV_KEY_END},
    {KEY_PAGEUP, K_KEY_PAGE_UP},
    {KEY_PAGEDOWN, K_KEY_PAGE_DOWN},
    {KEY_TAB, '\t'},
    {KEY_FN, K_KEY_FN},
    {KEY_LEFTCTRL, K_KEY_CTRL},
    {KEY_RIGHTCTRL, K_KEY_CTRL},
    {KEY_LEFTSHIFT, K_KEY_SHIFT},
    {KEY_RIGHTSHIFT, K_KEY_SHIFT},
    {KEY_LEFTMETA, K_KEY_SUPER},
    {KEY_RIGHTMETA, K_KEY_SUPER},
    {KEY_LEFTALT, K_KEY_ALT},
    {KEY_RIGHTALT, K_KEY_ALT},
    {KEY_MUTE, K_KEY_VOLUME_MUTE},
    {KEY_VOLUMEDOWN, K_KEY_VOLUME_DOWN},
    {KEY_VOLUMEUP, K_KEY_VOLUME_UP},
    {KEY_BRIGHTNESSDOWN, K_KEY_BRIGHTNESS_DOWN},
    {KEY_BRIGHTNESSUP, K_KEY_BRIGHTNESS_UP},
}};

constexpr std::array<KeyMapEntry, 12> K_FUNCTION_KEYS = {{
    {KEY_F1, K_KEY_F_BASE | 1U},
    {KEY_F2, K_KEY_F_BASE | 2U},
    {KEY_F3, K_KEY_F_BASE | 3U},
    {KEY_F4, K_KEY_F_BASE | 4U},
    {KEY_F5, K_KEY_F_BASE | 5U},
    {KEY_F6, K_KEY_F_BASE | 6U},
    {KEY_F7, K_KEY_F_BASE | 7U},
    {KEY_F8, K_KEY_F_BASE | 8U},
    {KEY_F9, K_KEY_F_BASE | 9U},
    {KEY_F10, K_KEY_F_BASE | 10U},
    {KEY_F11, K_KEY_F_BASE | 11U},
    {KEY_F12, K_KEY_F_BASE | 12U},
}};

constexpr std::array<KeyMapEntry, 2> K_SCAN_CODE_KEYS = {{
    {66, K_KEY_FN},
    {67, K_KEY_SYM},
}};

constexpr std::array<KeyMapEntry, 2> K_SHIFT_STATE_KEYS = {{
    {KEY_LEFTSHIFT, 1U},
    {KEY_RIGHTSHIFT, 1U},
}};

constexpr std::array<EvdevAlphaKeyRange, 3> K_ALPHA_KEY_RANGES = {{
    {KEY_Q, KEY_P, "qwertyuiop", "QWERTYUIOP"},
    {KEY_A, KEY_L, "asdfghjkl", "ASDFGHJKL"},
    {KEY_Z, KEY_M, "zxcvbnm", "ZXCVBNM"},
}};

uint32_t lookup_printable_key(uint16_t code, bool shift_pressed) {
  for (const auto& entry : K_PRINTABLE_SYMBOL_KEYS) {
    if (entry.code == code) {
      return shift_pressed ? entry.shifted_key : entry.key;
    }
  }
  return 0;
}

uint32_t lookup_alpha_key(uint16_t code, bool shift_pressed) {
  for (const auto& range : K_ALPHA_KEY_RANGES) {
    if (code >= range.first_code && code <= range.last_code) {
      return static_cast<uint32_t>(
          (shift_pressed ? range.shifted_keys : range.keys)[code - range.first_code]);
    }
  }
  return 0;
}

uint32_t printable_evdev_key(uint16_t code, bool shift_pressed) {
  if (const auto key = lookup_key(K_TCA8418_SYMBOL_KEYS, code)) {
    return key;
  }
  if (const auto key = lookup_printable_key(code, shift_pressed)) {
    return key;
  }
  if (const auto key = lookup_alpha_key(code, shift_pressed)) {
    return key;
  }

  return lookup_key(K_KEYPAD_SYMBOL_KEYS, code);
}

uint32_t map_evdev_key(uint16_t code, bool shift_pressed) {
  const auto printable = printable_evdev_key(code, shift_pressed);
  if (printable) {
    return printable;
  }

  if (const auto key = lookup_key(K_SPECIAL_EVDEV_KEYS, code)) {
    return key;
  }
  if (const auto key = lookup_key(K_FUNCTION_KEYS, code)) {
    return key;
  }

  return K_EVDEV_KEY_BASE | code;
}

uint32_t map_evdev_scan(uint32_t scan_code) {
  return lookup_key(K_SCAN_CODE_KEYS, static_cast<uint16_t>(scan_code));
}

void queue_router_event(EvdevKeypad* keypad, uint32_t key, bool pressed, const char* source) {
  if (!keypad) {
    return;
  }

  keypad->router_event_pending = true;
  keypad->router_key           = key;
  keypad->router_pressed       = pressed;
  LOG_VERBOSE("evdev: queue router source={} key={} state={}",
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
    LOG_VERBOSE("evdev: raw type={} code={} value={}", input.type, input.code, input.value);
    if (input.type == EV_MSC && input.code == MSC_SCAN) {
      const auto scan_key = map_evdev_scan(static_cast<uint32_t>(input.value));
      if (!scan_key) {
        LOG_VERBOSE("evdev: ignored MSC_SCAN value={}", input.value);
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
      LOG_VERBOSE("evdev: ignored repeat code={}", input.code);
      continue;
    }

    if (lookup_key(K_SHIFT_STATE_KEYS, input.code)) {
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

  LOG_VERBOSE("probing evdev input device: {}", path);
  const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    LOG_WARN("failed to open input device {}: {}", path, strerror(errno));
    LV_LOG_WARN("failed to open input device %s: %s", path, strerror(errno));
    return nullptr;
  }

  if (!has_keyboard_keys(fd)) {
    LOG_VERBOSE("input device does not expose expected keyboard keys: {}", path);
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

  LOG_VERBOSE("discovering key input devices under /dev/input");
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
  LOG_VERBOSE("attached key router to keypad input device");
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

void set_key_release_listener(KeyReleaseListener listener, void* user_data) {
  auto& state                          = key_router_state();
  state.key_release_listener           = listener;
  state.key_release_listener_user_data = user_data;
}

void clear_key_release_listener(KeyReleaseListener listener, void* user_data) {
  auto& state = key_router_state();
  if (state.key_release_listener == listener && state.key_release_listener_user_data == user_data) {
    state.key_release_listener           = nullptr;
    state.key_release_listener_user_data = nullptr;
  }
}

void set_long_key_listener(LongKeyListener listener, void* user_data) {
  auto& state                       = key_router_state();
  state.long_key_listener           = listener;
  state.long_key_listener_user_data = user_data;
}

void clear_long_key_listener(LongKeyListener listener, void* user_data) {
  auto& state = key_router_state();
  if (state.long_key_listener == listener && state.long_key_listener_user_data == user_data) {
    state.long_key_listener           = nullptr;
    state.long_key_listener_user_data = nullptr;
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
  LOG_VERBOSE("key router: trigger mode {} -> {}",
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
  if (const char* name = lookup_name(K_SPECIAL_KEY_NAMES, key)) {
    return name;
  }
  if (key >= 32 && key <= 126) {
    std::snprintf(buffer, sizeof(buffer), "%c", static_cast<char>(key));
    return buffer;
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
