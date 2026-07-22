/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "start_menu_model.h"

#include <algorithm>

#include "ui_const.h"

namespace model {
namespace {

constexpr std::size_t category_to_index(StartMenuCategory category) {
  return static_cast<std::size_t>(category);
}

const std::array<StartMenuItem, StartMenuModel::K_ITEM_COUNT>& start_menu_items() {
  static const std::array<StartMenuItem, StartMenuModel::K_ITEM_COUNT> ITEMS = {{
      {"Start Full Test", view::ICON_FLASK, StartMenuCategory::AUTO, AppPage::KEYBOARD_TEST, true},
      {"Language", view::ICON_TRANSLATE, StartMenuCategory::AUTO, AppPage::START, false},
      {"Device Information", view::ICON_INFO, StartMenuCategory::AUTO, AppPage::DEVICE_INFO, false},
      {"Power Information",
       view::ICON_LIGHTNING,
       StartMenuCategory::AUTO,
       AppPage::POWER_INFO,
       false},
      {"CPU Benchmark", view::ICON_CPU, StartMenuCategory::AUTO, AppPage::CPU_BENCHMARK, false},
      {"Mem Stress Test",
       view::ICON_MEMORY,
       StartMenuCategory::AUTO,
       AppPage::MEM_STRESS_TEST,
       false},
      {"SD Card Test", view::ICON_HARDDRIVE, StartMenuCategory::AUTO, AppPage::SD_CARD_TEST, false},

      {"Input Test", view::ICON_KEYBOARD, StartMenuCategory::PERIPH, AppPage::KEYBOARD_TEST, false},
      {"Display Test", view::ICON_MONITOR, StartMenuCategory::PERIPH, AppPage::LCD_TEST, false},
      {"Audio Test", view::ICON_MICROPHONE, StartMenuCategory::PERIPH, AppPage::AUDIO_TEST, false},
      {"Camera Test", view::ICON_CAMERA, StartMenuCategory::PERIPH, AppPage::CAMERA_TEST, false},
      {"IR Sender",
       view::ICON_PAPER_PLANE,
       StartMenuCategory::PERIPH,
       AppPage::IR_SEND_TEST,
       false},
      {"IR Receiver",
       view::ICON_ENVELOPE_OPEN,
       StartMenuCategory::PERIPH,
       AppPage::IR_RECEIVE_TEST,
       false},
      {"IMU Test", view::ICON_VECTOR_THREE, StartMenuCategory::PERIPH, AppPage::IMU_TEST, false},

      {"Wi-Fi", view::ICON_WIFI, StartMenuCategory::COMMS, AppPage::WIFI_TEST, false},
      {"Bluetooth", view::ICON_BLUETOOTH, StartMenuCategory::COMMS, AppPage::BT_TEST, false},
      {"Ethernet", view::ICON_ETHERNET, StartMenuCategory::COMMS, AppPage::ETH_TEST, false},
      {"USB", view::ICON_USB, StartMenuCategory::COMMS, AppPage::USB_TEST, false},
      {"HDMI", view::ICON_MONITOR, StartMenuCategory::COMMS, AppPage::HDMI_TEST, false},
      {"I2C", view::ICON_SCAN, StartMenuCategory::COMMS, AppPage::I2C_TEST, false},
      {"SPI", view::ICON_LINK_SIMPLE_HOR, StartMenuCategory::COMMS, AppPage::SPI_TEST, false},
      {"UART", view::ICON_BROADCAST, StartMenuCategory::COMMS, AppPage::UART_TEST, false},
      {"EXT.IO", view::ICON_PLUGS_CONNECTED, StartMenuCategory::COMMS, AppPage::EXT_IO_TEST, false},
      {"Link Test", view::ICON_GLOBE, StartMenuCategory::COMMS, AppPage::LINK_TEST, false},
      {"CAP LoRa-1262",
       view::ICON_BROADCAST,
       StartMenuCategory::COMMS,
       AppPage::CAP_LORA_1262_TEST,
       false},
      {"CAP-CC1101",
       view::ICON_BROADCAST,
       StartMenuCategory::COMMS,
       AppPage::CAP_CC1101_TEST,
       false},
  }};
  return ITEMS;
}

}  // namespace

const std::array<StartMenuItem, StartMenuModel::K_ITEM_COUNT>& StartMenuModel::items() const {
  return start_menu_items();
}

StartMenuCategory StartMenuModel::selected_category() const { return selected_category_; }

std::size_t StartMenuModel::selected_category_index() const {
  return category_to_index(selected_category_);
}

std::size_t StartMenuModel::selected_index() const {
  return selected_index_for_category_(selected_category_);
}

const StartMenuItem& StartMenuModel::selected_item() const {
  const auto absolute_index = absolute_index_for_category_(selected_category_, selected_index());
  return items()[absolute_index];
}

std::size_t StartMenuModel::item_count_for_category(StartMenuCategory category) const {
  std::size_t count = 0;
  for (const auto& item : items()) {
    if (item.category == category) {
      ++count;
    }
  }
  return count;
}

const StartMenuItem* StartMenuModel::item_for_category(StartMenuCategory category,
                                                       std::size_t index) const {
  const auto absolute_index = absolute_index_for_category_(category, index);
  if (absolute_index >= K_ITEM_COUNT) {
    return nullptr;
  }
  return &items()[absolute_index];
}

void StartMenuModel::select_previous() {
  auto& index = selected_indices_[category_to_index(selected_category_)];
  if (index > 0) {
    --index;
  }
}

void StartMenuModel::select_next() {
  auto& index      = selected_indices_[category_to_index(selected_category_)];
  const auto count = item_count_for_category(selected_category_);
  if (index + 1 < count) {
    ++index;
  }
}

void StartMenuModel::set_selected_index(std::size_t index) {
  const auto count = item_count_for_category(selected_category_);
  if (index < count) {
    selected_indices_[category_to_index(selected_category_)] = index;
  }
}

void StartMenuModel::select_previous_category() {
  const auto current = selected_category_index();
  if (current > 0) {
    set_selected_category(static_cast<StartMenuCategory>(current - 1));
  }
}

void StartMenuModel::select_next_category() {
  const auto current = selected_category_index();
  if (current + 1 < K_CATEGORY_COUNT) {
    set_selected_category(static_cast<StartMenuCategory>(current + 1));
  }
}

void StartMenuModel::set_selected_category(StartMenuCategory category) {
  const auto category_index = category_to_index(category);
  if (category_index >= K_CATEGORY_COUNT) {
    return;
  }

  selected_category_ = category;
  const auto count   = item_count_for_category(category);
  if (count == 0) {
    selected_indices_[category_index] = 0;
  } else {
    selected_indices_[category_index] = std::min(selected_indices_[category_index], count - 1);
  }
}

bool StartMenuModel::drawer_hidden() const { return drawer_hidden_; }

void StartMenuModel::set_drawer_hidden(bool hidden) { drawer_hidden_ = hidden; }

std::size_t StartMenuModel::absolute_index_for_category_(StartMenuCategory category,
                                                         std::size_t index) const {
  std::size_t category_index = 0;
  for (std::size_t i = 0; i < items().size(); ++i) {
    if (items()[i].category != category) {
      continue;
    }
    if (category_index == index) {
      return i;
    }
    ++category_index;
  }
  return K_ITEM_COUNT;
}

std::size_t StartMenuModel::selected_index_for_category_(StartMenuCategory category) const {
  const auto category_index = category_to_index(category);
  if (category_index >= K_CATEGORY_COUNT) {
    return 0;
  }
  return selected_indices_[category_index];
}

}  // namespace model
