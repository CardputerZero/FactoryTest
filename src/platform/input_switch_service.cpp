/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "input_switch_service.h"

#include <cstddef>
#include <mutex>
#include <string>

#include "logger.h"

#if defined(__linux__)
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace platform::input_switch {
namespace {

constexpr const char* K_INPUT_DIRECTORY  = "/dev/input";
constexpr const char* K_EVENT_PREFIX     = "event";
constexpr const char* K_HEADPHONE_DEVICE = "ES8389-Audio Headphones";

#if defined(__linux__)
constexpr std::size_t K_BITS_PER_WORD = sizeof(unsigned long) * 8U;

struct HeadphoneInputState {
  std::mutex mutex;
  int fd{-1};
  std::string path;

  ~HeadphoneInputState() {
    if (fd >= 0) {
      close(fd);
    }
  }
};

HeadphoneInputState& headphone_input_state() {
  static HeadphoneInputState state;
  return state;
}

bool bit_is_set(const unsigned long* bits, unsigned int code) {
  return (bits[code / K_BITS_PER_WORD] & (1UL << (code % K_BITS_PER_WORD))) != 0;
}

void close_device(HeadphoneInputState& state) {
  if (state.fd >= 0) {
    close(state.fd);
    state.fd = -1;
  }
  state.path.clear();
}

bool supports_headphone_switch(int fd) {
  unsigned long switch_bits[(SW_MAX / K_BITS_PER_WORD) + 1U] = {};
  if (ioctl(fd, EVIOCGBIT(EV_SW, sizeof(switch_bits)), switch_bits) < 0) {
    return false;
  }
  return bit_is_set(switch_bits, SW_HEADPHONE_INSERT);
}

bool discover_headphone_device(HeadphoneInputState& state, std::string& error_message) {
  DIR* directory = opendir(K_INPUT_DIRECTORY);
  if (!directory) {
    error_message =
        std::string("Failed to open ") + K_INPUT_DIRECTORY + ": " + std::strerror(errno);
    return false;
  }

  bool found = false;
  while (auto* entry = readdir(directory)) {
    if (std::strncmp(entry->d_name, K_EVENT_PREFIX, std::strlen(K_EVENT_PREFIX)) != 0) {
      continue;
    }

    const std::string path = std::string(K_INPUT_DIRECTORY) + "/" + entry->d_name;
    const int fd           = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
      continue;
    }

    char device_name[256]   = {};
    const bool name_matches = ioctl(fd, EVIOCGNAME(sizeof(device_name)), device_name) >= 0 &&
                              std::strcmp(device_name, K_HEADPHONE_DEVICE) == 0;
    if (!name_matches || !supports_headphone_switch(fd)) {
      close(fd);
      continue;
    }

    state.fd   = fd;
    state.path = path;
    found      = true;
    LOG_INFO("using headphone switch input device: {} ({})", state.path, device_name);
    break;
  }

  closedir(directory);
  if (!found) {
    error_message = std::string("Input device not found: ") + K_HEADPHONE_DEVICE;
  }
  return found;
}

bool read_current_switch_state(HeadphoneInputState& state,
                               bool& inserted,
                               std::string& error_message) {
  unsigned long switch_bits[(SW_MAX / K_BITS_PER_WORD) + 1U] = {};
  if (ioctl(state.fd, EVIOCGSW(sizeof(switch_bits)), switch_bits) < 0) {
    error_message = std::string("Failed to read headphone switch: ") + std::strerror(errno);
    return false;
  }

  inserted = bit_is_set(switch_bits, SW_HEADPHONE_INSERT);
  error_message.clear();
  return true;
}
#endif

}  // namespace

bool read_headphone_inserted(bool& inserted, std::string& error_message) {
#if defined(__linux__)
  auto& state = headphone_input_state();
  std::lock_guard<std::mutex> lock(state.mutex);

  if (state.fd < 0 && !discover_headphone_device(state, error_message)) {
    return false;
  }
  if (read_current_switch_state(state, inserted, error_message)) {
    return true;
  }

  close_device(state);
  if (!discover_headphone_device(state, error_message)) {
    return false;
  }
  return read_current_switch_state(state, inserted, error_message);
#else
  (void)inserted;
  error_message = "Headphone switch input is only available on Linux";
  return false;
#endif
}

}  // namespace platform::input_switch
