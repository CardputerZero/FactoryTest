/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app_viewmodel.h"

#include <array>
#include <utility>

namespace viewmodel {
namespace {

int page_to_int(model::AppPage page) { return static_cast<int>(page); }

struct TestSequenceItem {
  const char* name;
  model::AppPage page;
};

constexpr std::array<TestSequenceItem, 22> K_TEST_SEQUENCE = {{
    {"Input Test", model::AppPage::KEYBOARD_TEST},
    {"Display Test", model::AppPage::LCD_TEST},
    {"Audio Test", model::AppPage::AUDIO_TEST},
    {"Camera Test", model::AppPage::CAMERA_TEST},
    {"IR Sender", model::AppPage::IR_SEND_TEST},
    {"IR Receiver", model::AppPage::IR_RECEIVE_TEST},
    {"Wi-Fi", model::AppPage::WIFI_TEST},
    {"Bluetooth", model::AppPage::BT_TEST},
    {"Ethernet", model::AppPage::ETH_TEST},
    {"USB", model::AppPage::USB_TEST},
    {"HDMI", model::AppPage::HDMI_TEST},
    {"I2C", model::AppPage::I2C_TEST},
    {"SPI", model::AppPage::SPI_TEST},
    {"UART", model::AppPage::UART_TEST},
    {"EXT.IO", model::AppPage::EXT_IO_TEST},
    {"Link Test", model::AppPage::LINK_TEST},
    {"Device Information", model::AppPage::DEVICE_INFO},
    {"Power Information", model::AppPage::POWER_INFO},
    {"IMU Test", model::AppPage::IMU_TEST},
    {"CPU Benchmark", model::AppPage::CPU_BENCHMARK},
    {"Mem Stress Test", model::AppPage::MEM_STRESS_TEST},
    {"SD Card Test", model::AppPage::SD_CARD_TEST},
}};

const TestSequenceItem* sequence_item(std::size_t index) {
  if (index >= K_TEST_SEQUENCE.size()) {
    return nullptr;
  }
  return &K_TEST_SEQUENCE[index];
}

}  // namespace

AppViewModel::AppViewModel(model::TranslationService& translations)
    : translations_(translations),
      title_msgid_(model_.app_title()),
      title_subject_(translations_.translate(title_msgid_).c_str()),
      title_alignment_subject_(static_cast<int32_t>(LV_ALIGN_LEFT_MID)),
      title_x_offset_subject_(8),
      title_y_offset_subject_(0),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      quit_requested_subject_(false) {}

AppViewModel::~AppViewModel() = default;

lv_subject_t* AppViewModel::title_subject() { return title_subject_.native(); }

lv_subject_t* AppViewModel::title_alignment_subject() { return title_alignment_subject_.native(); }

lv_subject_t* AppViewModel::title_x_offset_subject() { return title_x_offset_subject_.native(); }

lv_subject_t* AppViewModel::title_y_offset_subject() { return title_y_offset_subject_.native(); }

lv_subject_t* AppViewModel::nav_actions_subject() { return nav_actions_subject_.native(); }

lv_subject_t* AppViewModel::dark_mode_subject() { return dark_mode_subject_.native(); }

lv_subject_t* AppViewModel::language_subject() { return language_subject_.native(); }

lv_subject_t* AppViewModel::current_page_subject() { return current_page_subject_.native(); }

lv_subject_t* AppViewModel::quit_requested_subject() { return quit_requested_subject_.native(); }

bool AppViewModel::is_dark_mode() const { return model_.dark_mode(); }

const std::string& AppViewModel::language() const { return translations_.language(); }

const std::array<NavAction, 5>& AppViewModel::nav_actions() const { return nav_actions_; }

void AppViewModel::set_dark_mode(bool enabled) {
  model_.set_dark_mode(enabled);
  dark_mode_subject_.set(model_.dark_mode());
}

void AppViewModel::toggle_dark_mode() {
  model_.toggle_dark_mode();
  dark_mode_subject_.set(model_.dark_mode());
}

bool AppViewModel::set_language(const std::string& locale) {
  if (!translations_.set_language(locale)) {
    return false;
  }

  title_subject_.set(translations_.translate(title_msgid_).c_str());
  language_subject_.set(language_subject_.value() + 1);
  return true;
}

std::string AppViewModel::tr(const char* msgid) const {
  return translations_.translate(msgid ? msgid : "");
}

const char* AppViewModel::ui_font_name(const char* latin_font_name) const {
  return translations_.uses_cjk_font() ? "alibaba-puhui-regular.ttf" : latin_font_name;
}

model::AppPage AppViewModel::current_page() const { return model_.current_page(); }

bool AppViewModel::is_test_sequence_active() const { return model_.test_sequence_active(); }

void AppViewModel::set_title_text(const char* title) {
  title_msgid_ = title ? title : "";
  title_subject_.set(translations_.translate(title_msgid_).c_str());
}

void AppViewModel::set_title_alignment(lv_align_t align, int32_t x_offset, int32_t y_offset) {
  title_alignment_subject_.set(static_cast<int32_t>(align));
  title_x_offset_subject_.set(x_offset);
  title_y_offset_subject_.set(y_offset);
}

void AppViewModel::set_nav_action(std::size_t index, NavAction action) {
  if (index >= nav_actions_.size()) {
    return;
  }
  nav_actions_[index] = std::move(action);
  nav_actions_subject_.set(++nav_actions_revision_);
}

void AppViewModel::set_keypad_nav_action(uint32_t keypad, NavAction action) {
  if (keypad < '4' || keypad > '8') {
    return;
  }
  set_nav_action(static_cast<std::size_t>(keypad - '4'), std::move(action));
}

void AppViewModel::set_nav_actions(std::array<NavAction, 5> actions) {
  nav_actions_ = std::move(actions);
  nav_actions_subject_.set(++nav_actions_revision_);
}

void AppViewModel::clear_nav_actions() {
  nav_actions_ = {};
  nav_actions_subject_.set(++nav_actions_revision_);
}

bool AppViewModel::trigger_nav_action(std::size_t index, lv_event_code_t event_code) {
  if (index >= nav_actions_.size()) {
    return false;
  }
  auto& action = nav_actions_[index];
  if (!action.action || action.event_code != event_code) {
    return false;
  }
  action.action();
  return true;
}

void AppViewModel::notify_nav_action_pressed(std::size_t index) {
  if (index >= nav_actions_.size() || !nav_actions_[index].press_action) {
    return;
  }
  nav_actions_[index].press_action();
}

void AppViewModel::notify_nav_action_released(std::size_t index) {
  if (index >= nav_actions_.size() || !nav_actions_[index].release_action) {
    return;
  }
  nav_actions_[index].release_action();
}

void AppViewModel::show_start_page() {
  model_.set_test_sequence_active(false);
  show_page_(model::AppPage::START);
}

void AppViewModel::show_keyboard_test_page() { show_page_(model::AppPage::KEYBOARD_TEST); }

void AppViewModel::show_lcd_test_page() { show_page_(model::AppPage::LCD_TEST); }

void AppViewModel::show_audio_test_page() { show_page_(model::AppPage::AUDIO_TEST); }

void AppViewModel::show_camera_test_page() { show_page_(model::AppPage::CAMERA_TEST); }

void AppViewModel::show_ir_send_test_page() { show_page_(model::AppPage::IR_SEND_TEST); }

void AppViewModel::show_ir_receive_test_page() { show_page_(model::AppPage::IR_RECEIVE_TEST); }

void AppViewModel::show_imu_test_page() { show_page_(model::AppPage::IMU_TEST); }

void AppViewModel::show_power_info_page() { show_page_(model::AppPage::POWER_INFO); }

void AppViewModel::show_device_info_page() { show_page_(model::AppPage::DEVICE_INFO); }

void AppViewModel::show_perf_test_page() { show_page_(model::AppPage::PERF_TEST); }

void AppViewModel::show_test_result_page() { show_page_(model::AppPage::TEST_RESULT); }

void AppViewModel::start_full_test_sequence() {
  model_.set_test_sequence_active(true);
  test_sequence_index_ = 0;
  test_session_.start();
  if (const auto* item = sequence_item(test_sequence_index_)) {
    show_page_(item->page);
  } else {
    show_start_page();
  }
}

void AppViewModel::show_single_test_page(model::AppPage page) {
  model_.set_test_sequence_active(false);
  show_page_(page);
}

void AppViewModel::refresh_current_page() { current_page_subject_.notify(); }

void AppViewModel::complete_current_test() { complete_current_test(model::TestResult::PASS); }

void AppViewModel::complete_current_test(model::TestResult result) {
  if (model_.test_sequence_active()) {
    test_session_.append_result(current_test_name(), result);
  }

  if (!model_.test_sequence_active()) {
    show_start_page();
    return;
  }

  ++test_sequence_index_;
  if (const auto* item = sequence_item(test_sequence_index_)) {
    show_page_(item->page);
    return;
  }

  model_.set_test_sequence_active(false);
  show_test_result_page();
}

const char* AppViewModel::current_test_name() const {
  if (model_.test_sequence_active()) {
    if (const auto* item = sequence_item(test_sequence_index_)) {
      return item->name;
    }
  }

  switch (model_.current_page()) {
    case model::AppPage::KEYBOARD_TEST:
      return "Input Test";
    case model::AppPage::LCD_TEST:
      return "Display Test";
    case model::AppPage::AUDIO_TEST:
      return "Audio Test";
    case model::AppPage::CAMERA_TEST:
      return "Camera Test";
    case model::AppPage::IR_SEND_TEST:
      return "IR Sender";
    case model::AppPage::IR_RECEIVE_TEST:
      return "IR Receiver";
    case model::AppPage::WIFI_TEST:
      return "Wi-Fi";
    case model::AppPage::BT_TEST:
      return "Bluetooth";
    case model::AppPage::ETH_TEST:
      return "Ethernet";
    case model::AppPage::USB_TEST:
      return "USB";
    case model::AppPage::HDMI_TEST:
      return "HDMI";
    case model::AppPage::I2C_TEST:
      return "I2C";
    case model::AppPage::SPI_TEST:
      return "SPI";
    case model::AppPage::UART_TEST:
      return "UART";
    case model::AppPage::EXT_IO_TEST:
      return "EXT.IO";
    case model::AppPage::LINK_TEST:
      return "Link Test";
    case model::AppPage::POWER_INFO:
      return "Power Information";
    case model::AppPage::IMU_TEST:
      return "IMU Test";
    case model::AppPage::DEVICE_INFO:
      return "Device Information";
    case model::AppPage::PERF_TEST:
      return "Performance Test";
    case model::AppPage::CPU_BENCHMARK:
      return "CPU Benchmark";
    case model::AppPage::MEM_STRESS_TEST:
      return "Mem Stress Test";
    case model::AppPage::SD_CARD_TEST:
      return "SD Card Test";
    case model::AppPage::START:
    case model::AppPage::TEST_RESULT:
    default:
      return "Factory Test";
  }
}

std::size_t AppViewModel::current_test_number() const {
  if (!model_.test_sequence_active() || test_sequence_index_ >= K_TEST_SEQUENCE.size()) {
    return 0;
  }
  return test_sequence_index_ + 1;
}

std::size_t AppViewModel::test_count() const { return K_TEST_SEQUENCE.size(); }

const std::string& AppViewModel::test_result_path() const { return test_session_.output_path(); }

void AppViewModel::set_back_request_handler(BackRequestHandler handler, void* user_data) {
  back_request_handler_   = handler;
  back_request_user_data_ = user_data;
}

void AppViewModel::clear_back_request_handler(BackRequestHandler handler, void* user_data) {
  if (back_request_handler_ == handler && back_request_user_data_ == user_data) {
    back_request_handler_   = nullptr;
    back_request_user_data_ = nullptr;
  }
}

void AppViewModel::request_back_or_quit() {
  if (back_request_handler_ && back_request_handler_(back_request_user_data_)) {
    return;
  }

  if (model_.current_page() == model::AppPage::START) {
    request_quit();
    return;
  }

  show_start_page();
}

void AppViewModel::request_quit() { quit_requested_subject_.set(true); }

void AppViewModel::publish_all_() {
  title_msgid_ = model_.app_title();
  title_subject_.set(translations_.translate(title_msgid_).c_str());
  dark_mode_subject_.set(model_.dark_mode());
  current_page_subject_.set(page_to_int(model_.current_page()));
}

void AppViewModel::show_page_(model::AppPage page) {
  clear_nav_actions();
  set_title_alignment(LV_ALIGN_LEFT_MID, 8, 0);
  model_.set_current_page(page);
  publish_all_();
}

}  // namespace viewmodel
