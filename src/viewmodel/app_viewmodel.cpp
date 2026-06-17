/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app_viewmodel.h"

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
      return model::AppPage::IMU_TEST;
    case model::AppPage::IMU_TEST:
      return model::AppPage::POWER_INFO;
    case model::AppPage::POWER_INFO:
      return model::AppPage::DEVICE_INFO;
    case model::AppPage::DEVICE_INFO:
    case model::AppPage::START:
      return model::AppPage::START;
  }

  return model::AppPage::START;
}

}  // namespace

AppViewModel::AppViewModel()
    : title_subject_(model_.app_title()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      quit_requested_subject_(false) {}

AppViewModel::~AppViewModel() = default;

lv_subject_t* AppViewModel::title_subject() { return title_subject_.native(); }

lv_subject_t* AppViewModel::dark_mode_subject() { return dark_mode_subject_.native(); }

lv_subject_t* AppViewModel::current_page_subject() { return current_page_subject_.native(); }

lv_subject_t* AppViewModel::quit_requested_subject() { return quit_requested_subject_.native(); }

bool AppViewModel::is_dark_mode() const { return model_.dark_mode(); }

void AppViewModel::set_dark_mode(bool enabled) {
  model_.set_dark_mode(enabled);
  publish_all_();
}

void AppViewModel::toggle_dark_mode() {
  model_.toggle_dark_mode();
  publish_all_();
}

model::AppPage AppViewModel::current_page() const { return model_.current_page(); }

bool AppViewModel::is_test_sequence_active() const { return model_.test_sequence_active(); }

void AppViewModel::show_start_page() {
  model_.set_test_sequence_active(false);
  show_page_(model::AppPage::START);
}

void AppViewModel::show_keyboard_test_page() { show_page_(model::AppPage::KEYBOARD_TEST); }

void AppViewModel::show_lcd_test_page() { show_page_(model::AppPage::LCD_TEST); }

void AppViewModel::show_audio_test_page() { show_page_(model::AppPage::AUDIO_TEST); }

void AppViewModel::show_camera_test_page() { show_page_(model::AppPage::CAMERA_TEST); }

void AppViewModel::show_connectivity_test_page() { show_page_(model::AppPage::CONNECTIVITY_TEST); }

void AppViewModel::show_imu_test_page() { show_page_(model::AppPage::IMU_TEST); }

void AppViewModel::show_power_info_page() { show_page_(model::AppPage::POWER_INFO); }

void AppViewModel::show_device_info_page() { show_page_(model::AppPage::DEVICE_INFO); }

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
  model_.set_current_page(page);
  publish_all_();
}

}  // namespace viewmodel
