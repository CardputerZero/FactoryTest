/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "app_model.h"
#include "lvgl.h"
#include "subjects.h"

namespace viewmodel {

using BackRequestHandler = bool (*)(void* user_data);

class AppViewModel {
 public:
  AppViewModel();
  ~AppViewModel();

  AppViewModel(const AppViewModel&)            = delete;
  AppViewModel& operator=(const AppViewModel&) = delete;

  lv_subject_t* title_subject();
  lv_subject_t* dark_mode_subject();
  lv_subject_t* current_page_subject();
  lv_subject_t* quit_requested_subject();

  bool is_dark_mode() const;
  void set_dark_mode(bool enabled);
  void toggle_dark_mode();

  model::AppPage current_page() const;
  bool is_test_sequence_active() const;
  void show_start_page();
  void show_keyboard_test_page();
  void show_lcd_test_page();
  void show_audio_test_page();
  void show_camera_test_page();
  void show_connectivity_test_page();
  void show_imu_test_page();
  void show_power_info_page();
  void show_device_info_page();
  void start_full_test_sequence();
  void show_single_test_page(model::AppPage page);
  void complete_current_test();
  void set_back_request_handler(BackRequestHandler handler, void* user_data);
  void clear_back_request_handler(BackRequestHandler handler, void* user_data);
  void request_back_or_quit();
  void request_quit();

 protected:
  void publish_all_();
  void show_page_(model::AppPage page);

 private:
  model::AppModel model_{};
  reactive::StringSubject<48> title_subject_;
  reactive::BoolSubject dark_mode_subject_;
  reactive::IntSubject current_page_subject_;
  reactive::BoolSubject quit_requested_subject_;
  BackRequestHandler back_request_handler_{nullptr};
  void* back_request_user_data_{nullptr};
};

}  // namespace viewmodel
