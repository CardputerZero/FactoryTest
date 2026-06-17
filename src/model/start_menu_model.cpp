/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "start_menu_model.h"

namespace model {
namespace {

const std::array<StartMenuItem, StartMenuModel::K_ITEM_COUNT>& start_menu_items() {
  static const std::array<StartMenuItem, StartMenuModel::K_ITEM_COUNT> ITEMS = {{
      {"Start Full Test", AppPage::KEYBOARD_TEST, true},
      {"Input Test", AppPage::KEYBOARD_TEST, false},
      {"Display Test", AppPage::LCD_TEST, false},
      {"Audio Test", AppPage::AUDIO_TEST, false},
      {"Camera Test", AppPage::CAMERA_TEST, false},
      {"Connectivity Test", AppPage::CONNECTIVITY_TEST, false},
      {"Power Information", AppPage::POWER_INFO, false},
      {"IMU Test", AppPage::IMU_TEST, false},
      {"Device Information", AppPage::DEVICE_INFO, false},
  }};
  return ITEMS;
}

}  // namespace

const std::array<StartMenuItem, StartMenuModel::K_ITEM_COUNT>& StartMenuModel::items() const {
  return start_menu_items();
}

std::size_t StartMenuModel::selected_index() const { return selected_index_; }

void StartMenuModel::select_previous() {
  if (selected_index_ > 0) {
    --selected_index_;
  }
}

void StartMenuModel::select_next() {
  if (selected_index_ + 1 < K_ITEM_COUNT) {
    ++selected_index_;
  }
}

void StartMenuModel::set_selected_index(std::size_t index) {
  if (index < K_ITEM_COUNT) {
    selected_index_ = index;
  }
}

}  // namespace model
