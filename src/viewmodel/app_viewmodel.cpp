/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app_viewmodel.h"

#include <utility>

namespace viewmodel {
namespace {

int page_to_int(model::AppPage page) { return static_cast<int>(page); }

model::AppPage next_test_page(model::AppPage page) {
  switch (page) {
    case model::AppPage::KEYBOARD_TEST:
      return model::AppPage::LCD_TEST;
    case model::AppPage::LCD_TEST:
      return model::AppPage::AUDIO_TEST;
    case model::AppPage::AUDIO_TEST:
      return model::AppPage::CAMERA_TEST;
    case model::AppPage::CAMERA_TEST:
      return model::AppPage::CONNECTIVITY_TEST;
    case model::AppPage::CONNECTIVITY_TEST:
      return model::AppPage::IR_TEST;
    case model::AppPage::IR_TEST:
      return model::AppPage::POWER_INFO;
    case model::AppPage::POWER_INFO:
      return model::AppPage::IMU_TEST;
    case model::AppPage::IMU_TEST:
      return model::AppPage::DEVICE_INFO;
    case model::AppPage::DEVICE_INFO:
      return model::AppPage::PERF_TEST;
    case model::AppPage::PERF_TEST:
    case model::AppPage::START:
      return model::AppPage::START;
  }

  return model::AppPage::START;
}

}  // namespace

AppViewModel::AppViewModel()
    : title_subject_(model_.app_title()),
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

lv_subject_t* AppViewModel::current_page_subject() { return current_page_subject_.native(); }

lv_subject_t* AppViewModel::quit_requested_subject() { return quit_requested_subject_.native(); }

bool AppViewModel::is_dark_mode() const { return model_.dark_mode(); }

const std::array<NavAction, 5>& AppViewModel::nav_actions() const { return nav_actions_; }

void AppViewModel::set_dark_mode(bool enabled) {
  model_.set_dark_mode(enabled);
  dark_mode_subject_.set(model_.dark_mode());
}

void AppViewModel::toggle_dark_mode() {
  model_.toggle_dark_mode();
  dark_mode_subject_.set(model_.dark_mode());
}

model::AppPage AppViewModel::current_page() const { return model_.current_page(); }

bool AppViewModel::is_test_sequence_active() const { return model_.test_sequence_active(); }

void AppViewModel::set_title_text(const char* title) { title_subject_.set(title ? title : ""); }

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

void AppViewModel::show_connectivity_test_page() { show_page_(model::AppPage::CONNECTIVITY_TEST); }

void AppViewModel::show_ir_test_page() { show_page_(model::AppPage::IR_TEST); }

void AppViewModel::show_imu_test_page() { show_page_(model::AppPage::IMU_TEST); }

void AppViewModel::show_power_info_page() { show_page_(model::AppPage::POWER_INFO); }

void AppViewModel::show_device_info_page() { show_page_(model::AppPage::DEVICE_INFO); }

void AppViewModel::show_perf_test_page() { show_page_(model::AppPage::PERF_TEST); }

void AppViewModel::start_full_test_sequence() {
  model_.set_test_sequence_active(true);
  show_keyboard_test_page();
}

void AppViewModel::show_single_test_page(model::AppPage page) {
  model_.set_test_sequence_active(false);
  show_page_(page);
}

void AppViewModel::complete_current_test() {
  if (!model_.test_sequence_active()) {
    show_start_page();
    return;
  }

  const auto next_page = next_test_page(model_.current_page());
  if (next_page == model::AppPage::START) {
    show_start_page();
    return;
  }

  show_page_(next_page);
}

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
  title_subject_.set(model_.app_title());
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
