/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app_model.h"

namespace model {

const char* AppModel::app_title() const {
  switch (current_page_) {
    case AppPage::START:
      return "Factory Test";
    case AppPage::KEYBOARD_TEST:
      return "Input Test";
    case AppPage::LCD_TEST:
      return "Display Test";
    case AppPage::AUDIO_TEST:
      return "Audio Test";
    case AppPage::CAMERA_TEST:
      return "Camera Test";
    case AppPage::CONNECTIVITY_TEST:
      return "Connectivity Test";
    case AppPage::IR_TEST:
      return "IR Test";
    case AppPage::POWER_INFO:
      return "Power Information";
    case AppPage::IMU_TEST:
      return "IMU Test";
    case AppPage::DEVICE_INFO:
      return "Device Information";
    case AppPage::PERF_TEST:
      return "Performance Test";
    case AppPage::WIFI_TEST:
      return "Wi-Fi";
    case AppPage::BT_TEST:
      return "Bluetooth";
    case AppPage::ETH_TEST:
      return "Ethernet";
    case AppPage::USB_TEST:
      return "USB";
    case AppPage::HDMI_TEST:
      return "HDMI";
    case AppPage::I2C_TEST:
      return "I2C";
    case AppPage::SPI_TEST:
      return "SPI";
    case AppPage::UART_TEST:
      return "UART";
    case AppPage::EXT_IO_TEST:
      return "EXT.IO";
    case AppPage::LINK_TEST:
      return "Link Test";
    case AppPage::CPU_BENCHMARK:
      return "CPU Benchmark";
    case AppPage::MEM_STRESS_TEST:
      return "Mem Stress Test";
    case AppPage::SD_CARD_TEST:
      return "SD Card Test";
    case AppPage::TEST_RESULT:
      return "Test Result";
  }

  return "Factory Test";
}

bool AppModel::dark_mode() const { return dark_mode_; }

void AppModel::set_dark_mode(bool enabled) { dark_mode_ = enabled; }

void AppModel::toggle_dark_mode() { dark_mode_ = !dark_mode_; }

AppPage AppModel::current_page() const { return current_page_; }

void AppModel::set_current_page(AppPage page) { current_page_ = page; }

bool AppModel::test_sequence_active() const { return test_sequence_active_; }

void AppModel::set_test_sequence_active(bool active) { test_sequence_active_ = active; }

}  // namespace model
