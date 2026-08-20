/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace precheck {

class UsbKeyboardService {
 public:
  using KeyCallback = void (*)(uint32_t key, bool pressed, void* user_data);

  UsbKeyboardService() = default;
  ~UsbKeyboardService();

  UsbKeyboardService(const UsbKeyboardService&) = delete;
  UsbKeyboardService& operator=(const UsbKeyboardService&) = delete;

  bool start(lv_display_t* display, KeyCallback callback, void* user_data);
  void stop();
  bool available() const { return fd_ >= 0; }

 private:
  static void poll_timer_cb(lv_timer_t* timer);
  static void indev_read_cb(lv_indev_t* indev, lv_indev_data_t* data);
  void poll_();
  void read_(lv_indev_data_t* data);
  void disconnect_();

  int fd_{-1};
  bool exclusive_grabbed_{false};
  uint32_t last_probe_at_{0};
  lv_timer_t* timer_{nullptr};
  lv_indev_t* indev_{nullptr};
  KeyCallback callback_{nullptr};
  void* user_data_{nullptr};
  uint32_t current_key_{0};
  bool current_pressed_{false};
};

}  // namespace precheck
