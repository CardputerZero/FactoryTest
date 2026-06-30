/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace model {

enum class AppPage {
  START           = 0,
  KEYBOARD_TEST   = 1,
  LCD_TEST        = 2,
  AUDIO_TEST      = 3,
  CAMERA_TEST     = 4,
  CONNECTIVITY_TEST = 5,
  IR_TEST         = 6,
  POWER_INFO      = 7,
  IMU_TEST        = 8,
  DEVICE_INFO     = 9,
  PERF_TEST       = 10,
  WIFI_TEST       = 11,
  BT_TEST         = 12,
  ETH_TEST        = 13,
  USB_TEST        = 14,
  HDMI_TEST       = 15,
  I2C_TEST        = 16,
  SPI_TEST        = 17,
  UART_TEST       = 18,
  EXT_IO_TEST     = 19,
  LINK_TEST       = 20,
  CPU_BENCHMARK   = 21,
  MEM_STRESS_TEST = 22,
  SD_CARD_TEST    = 23,
  TEST_RESULT     = 24,
};

class AppModel {
 public:
  const char* app_title() const;

  bool dark_mode() const;
  void set_dark_mode(bool enabled);
  void toggle_dark_mode();

  AppPage current_page() const;
  void set_current_page(AppPage page);

  bool test_sequence_active() const;
  void set_test_sequence_active(bool active);

 private:
  bool dark_mode_{true};
  AppPage current_page_{AppPage::START};
  bool test_sequence_active_{false};
};

}  // namespace model
