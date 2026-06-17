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
      return "CardputerZero Factory Test";
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
    case AppPage::IMU_TEST:
      return "IMU Test";
    case AppPage::POWER_INFO:
      return "Power Information";
    case AppPage::DEVICE_INFO:
      return "Device Information";
  }

  return "CardputerZero Factory Test";
}

bool AppModel::dark_mode() const { return dark_mode_; }

void AppModel::set_dark_mode(bool enabled) { dark_mode_ = enabled; }

void AppModel::toggle_dark_mode() { dark_mode_ = !dark_mode_; }

AppPage AppModel::current_page() const { return current_page_; }

void AppModel::set_current_page(AppPage page) { current_page_ = page; }

bool AppModel::test_sequence_active() const { return test_sequence_active_; }

void AppModel::set_test_sequence_active(bool active) { test_sequence_active_ = active; }

}  // namespace model
