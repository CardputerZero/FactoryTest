/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "backlight_service.h"

#include <algorithm>
#include <fstream>

#include "logger.h"

#ifndef APP_BACKLIGHT_BRIGHTNESS_PATH
#define APP_BACKLIGHT_BRIGHTNESS_PATH "/sys/class/backlight/backlight/brightness"
#endif

namespace platform::backlight {
namespace {

constexpr int32_t K_MIN_BRIGHTNESS = 0;
constexpr int32_t K_MAX_BRIGHTNESS = 100;

int32_t clamp_percent(int32_t percent) {
  return std::clamp(percent, K_MIN_BRIGHTNESS, K_MAX_BRIGHTNESS);
}

}  // namespace

bool read_brightness_percent(int32_t& percent) {
  std::ifstream file(APP_BACKLIGHT_BRIGHTNESS_PATH);
  if (!file) {
    static bool logged = false;
    if (!logged) {
      LOG_WARN("failed to open backlight brightness for reading: {}",
               APP_BACKLIGHT_BRIGHTNESS_PATH);
      logged = true;
    }
    return false;
  }

  int32_t value = 0;
  file >> value;
  if (!file) {
    LOG_WARN("failed to parse backlight brightness: {}", APP_BACKLIGHT_BRIGHTNESS_PATH);
    return false;
  }

  percent = clamp_percent(value);
  return true;
}

bool write_brightness_percent(int32_t percent) {
  std::ofstream file(APP_BACKLIGHT_BRIGHTNESS_PATH);
  if (!file) {
    static bool logged = false;
    if (!logged) {
      LOG_WARN("failed to open backlight brightness for writing: {}",
               APP_BACKLIGHT_BRIGHTNESS_PATH);
      logged = true;
    }
    return false;
  }

  file << clamp_percent(percent) << '\n';
  const bool ok = static_cast<bool>(file);
  if (!ok) {
    LOG_WARN("failed to write backlight brightness: {}", APP_BACKLIGHT_BRIGHTNESS_PATH);
  }
  return ok;
}

}  // namespace platform::backlight
