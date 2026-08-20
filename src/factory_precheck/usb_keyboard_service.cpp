/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "usb_keyboard_service.h"

#if !USE_DESKTOP
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <fcntl.h>
#include <linux/input.h>
#include <string>
#include <sys/ioctl.h>
#include <unordered_set>
#include <unistd.h>

#include "logger.h"
#endif

namespace precheck {
#if !USE_DESKTOP
namespace {

struct KeyboardCapabilities {
  int alphabetic_keys{0};
  int numeric_keys{0};
  bool enter{false};
  bool escape{false};
  bool space{false};
};

bool has_keyboard_keys(int fd, KeyboardCapabilities* capabilities) {
  unsigned long key_bits[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) return false;
  const auto bits_per_word = static_cast<int>(sizeof(unsigned long) * 8);
  const auto has = [&](int code) {
    return (key_bits[code / bits_per_word] & (1UL << (code % bits_per_word))) != 0;
  };

  KeyboardCapabilities detected;
  for (int code = KEY_A; code <= KEY_Z; ++code) {
    if (has(code)) ++detected.alphabetic_keys;
  }
  for (int code = KEY_0; code <= KEY_9; ++code) {
    if (has(code)) ++detected.numeric_keys;
  }
  detected.enter = has(KEY_ENTER) || has(KEY_KPENTER);
  detected.escape = has(KEY_ESC);
  detected.space = has(KEY_SPACE);
  if (capabilities) *capabilities = detected;

  // Device names vary by vendor. A real keyboard exposes several printable
  // keys plus standard controls; this avoids selecting mice, consumer
  // control endpoints, and gamepad buttons that also use EV_KEY.
  return detected.alphabetic_keys >= 6 && detected.enter &&
         (detected.escape || detected.space);
}

bool is_usb_input_device(const char* event_name) {
  if (!event_name || event_name[0] == '\0') return false;
  std::error_code error;
  const auto sysfs_path = std::filesystem::weakly_canonical(
      std::filesystem::path("/sys/class/input") / event_name / "device", error);
  if (error || sysfs_path.empty()) return false;

  for (const auto& component : sysfs_path) {
    const auto name = component.string();
    if (name == "usb" || name.rfind("usb", 0) == 0) return true;
  }
  return false;
}

uint32_t map_key(uint16_t code) {
  switch (code) {
    case KEY_ENTER:
    case KEY_KPENTER: return LV_KEY_ENTER;
    case KEY_ESC: return LV_KEY_ESC;
    case KEY_UP: return LV_KEY_UP;
    case KEY_DOWN: return LV_KEY_DOWN;
    case KEY_LEFT: return LV_KEY_LEFT;
    case KEY_RIGHT: return LV_KEY_RIGHT;
    case KEY_BACKSPACE: return LV_KEY_BACKSPACE;
    case KEY_DELETE: return LV_KEY_DEL;
    case KEY_SPACE: return ' ';
    case KEY_Q: return 'q';
    case KEY_R: return 'r';
    case KEY_P: return 'p';
    default: return 0;
  }
}

int open_usb_keyboard() {
  static std::unordered_set<std::string> logged_devices;
  static std::unordered_set<std::string> logged_grab_failures;
  static std::unordered_set<std::string> logged_non_usb_keyboards;
  static bool logged_input_directory_error = false;
  auto* dir = opendir("/dev/input");
  if (!dir) {
    if (!logged_input_directory_error) {
      LOG_ERROR("failed to open /dev/input: {}", std::strerror(errno));
      logged_input_directory_error = true;
    }
    return -1;
  }
  logged_input_directory_error = false;

  int selected = -1;
  while (auto* entry = readdir(dir)) {
    if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
    std::string path = "/dev/input/";
    path += entry->d_name;
    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    KeyboardCapabilities capabilities;
    if (fd < 0 || !has_keyboard_keys(fd, &capabilities)) {
      if (fd >= 0) close(fd);
      continue;
    }
    char name[256] = {};
    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
      std::strncpy(name, "<unknown>", sizeof(name) - 1);
    }
    const std::string identity = path + "|" + name;
    if (!is_usb_input_device(entry->d_name)) {
      if (logged_non_usb_keyboards.insert(identity).second) {
        LOG_INFO("keyboard ignored because it is not on USB bus: path='{}' name='{}'",
                 path,
                 name);
      }
      close(fd);
      continue;
    }
    if (logged_devices.insert(identity).second) {
      LOG_INFO("USB keyboard recognized: path='{}' name='{}' alphabetic_keys={} numeric_keys={} "
               "enter={} escape={} space={}",
               path,
               name,
               capabilities.alphabetic_keys,
               capabilities.numeric_keys,
               capabilities.enter ? "yes" : "no",
               capabilities.escape ? "yes" : "no",
               capabilities.space ? "yes" : "no");
    }
    if (ioctl(fd, EVIOCGRAB, 1) < 0) {
      if (logged_grab_failures.insert(identity).second) {
        LOG_WARN("failed to exclusively grab USB keyboard: path='{}' name='{}' err={}",
                 path,
                 name,
                 std::strerror(errno));
      }
      close(fd);
      continue;
    }
    selected = fd;
    LOG_INFO("USB keyboard opened exclusively: path='{}' name='{}'", path, name);
    break;
  }
  closedir(dir);
  return selected;
}

}  // namespace
#endif

UsbKeyboardService::~UsbKeyboardService() { stop(); }

bool UsbKeyboardService::start(lv_display_t* display,
                               KeyCallback callback,
                               void* user_data) {
#if USE_DESKTOP
  LV_UNUSED(display);
  LV_UNUSED(callback);
  LV_UNUSED(user_data);
  return false;
#else
  stop();
  callback_ = callback;
  user_data_ = user_data;
  fd_ = open_usb_keyboard();
  exclusive_grabbed_ = fd_ >= 0;
  indev_ = lv_indev_create();
  if (!indev_) {
    LOG_ERROR("failed to create LVGL USB keyboard input device");
    stop();
    return false;
  }
  lv_indev_set_type(indev_, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(indev_, indev_read_cb);
  lv_indev_set_user_data(indev_, this);
  lv_indev_set_display(indev_, display);
  timer_ = lv_timer_create(poll_timer_cb, 10, this);
  if (!timer_) {
    stop();
    return false;
  }
  last_probe_at_ = lv_tick_get();
  LV_UNUSED(display);
  return true;
#endif
}

void UsbKeyboardService::stop() {
  if (timer_) {
    lv_timer_delete(timer_);
    timer_ = nullptr;
  }
  if (indev_) {
    lv_indev_delete(indev_);
    indev_ = nullptr;
  }
#if !USE_DESKTOP
  if (fd_ >= 0) {
    if (exclusive_grabbed_ && ioctl(fd_, EVIOCGRAB, 0) < 0 && errno != ENODEV) {
      LOG_WARN("failed to release precheck USB keyboard grab: {}", std::strerror(errno));
    }
    close(fd_);
    fd_ = -1;
  }
#else
  fd_ = -1;
#endif
  exclusive_grabbed_ = false;
  last_probe_at_ = 0;
  callback_ = nullptr;
  user_data_ = nullptr;
  current_key_ = 0;
  current_pressed_ = false;
}

void UsbKeyboardService::poll_timer_cb(lv_timer_t* timer) {
  auto* service = static_cast<UsbKeyboardService*>(lv_timer_get_user_data(timer));
  if (service) service->poll_();
}

void UsbKeyboardService::indev_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
  data->continue_reading = false;
  auto* service = static_cast<UsbKeyboardService*>(lv_indev_get_user_data(indev));
  if (service) {
    service->read_(data);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    data->key = 0;
  }
}

void UsbKeyboardService::disconnect_() {
#if !USE_DESKTOP
  if (fd_ >= 0) {
    if (exclusive_grabbed_ && ioctl(fd_, EVIOCGRAB, 0) < 0 && errno != ENODEV) {
      LOG_WARN("failed to release disconnected precheck USB keyboard grab: {}",
               std::strerror(errno));
    }
    close(fd_);
    fd_ = -1;
  }
#endif
  exclusive_grabbed_ = false;
  current_key_ = 0;
  current_pressed_ = false;
  last_probe_at_ = lv_tick_get();
}

void UsbKeyboardService::read_(lv_indev_data_t* data) {
#if !USE_DESKTOP
  data->continue_reading = false;
  if (fd_ >= 0) {
    input_event event{};
    while (true) {
      const auto bytes = read(fd_, &event, sizeof(event));
      if (bytes != static_cast<ssize_t>(sizeof(event))) {
        if (bytes == 0 || (bytes < 0 && (errno == ENODEV || errno == EIO || errno == ENXIO))) {
          LOG_WARN("precheck USB keyboard disconnected; releasing exclusive grab");
          disconnect_();
        }
        break;
      }
      if (event.type != EV_KEY || event.value == 2) continue;
      const auto key = map_key(event.code);
      if (!key) continue;
      current_key_ = key;
      current_pressed_ = event.value != 0;
      if (callback_) callback_(current_key_, current_pressed_, user_data_);
      break;
    }
  }
  data->key = current_key_;
  data->state = current_pressed_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
#else
  LV_UNUSED(data);
#endif
}

void UsbKeyboardService::poll_() {
#if !USE_DESKTOP
  if (fd_ < 0) {
    if (lv_tick_elaps(last_probe_at_) >= 500) {
      last_probe_at_ = lv_tick_get();
      fd_ = open_usb_keyboard();
      exclusive_grabbed_ = fd_ >= 0;
    }
  }
#endif
}

}  // namespace precheck
