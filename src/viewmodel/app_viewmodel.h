/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "app_model.h"
#include "lvgl.h"
#include "subjects.h"
#include "test_session.h"
#include "translation_service.h"

namespace viewmodel {

using BackRequestHandler = bool (*)(void* user_data);

enum class NavHoldTarget {
  NONE,
  SUCCESS,
  ERROR,
};

struct NavAction {
  std::string icon{};
  std::function<void()> action{};
  std::function<void()> press_action{};
  std::function<void()> release_action{};
  lv_event_code_t event_code{LV_EVENT_CLICKED};
  NavHoldTarget hold_target{NavHoldTarget::NONE};
  bool force_enabled{false};
};

class AppViewModel {
 public:
  explicit AppViewModel(model::TranslationService& translations);
  ~AppViewModel();

  AppViewModel(const AppViewModel&)            = delete;
  AppViewModel& operator=(const AppViewModel&) = delete;

  lv_subject_t* title_subject();
  lv_subject_t* title_alignment_subject();
  lv_subject_t* title_x_offset_subject();
  lv_subject_t* title_y_offset_subject();
  lv_subject_t* nav_actions_subject();
  lv_subject_t* dark_mode_subject();
  lv_subject_t* language_subject();
  lv_subject_t* current_page_subject();
  lv_subject_t* quit_requested_subject();

  bool is_dark_mode() const;
  const std::string& language() const;
  const std::array<NavAction, 5>& nav_actions() const;
  void set_dark_mode(bool enabled);
  void toggle_dark_mode();
  bool set_language(const std::string& locale);
  std::string tr(const char* msgid) const;
  const char* ui_font_name(const char* latin_font_name) const;

  model::AppPage current_page() const;
  bool is_test_sequence_active() const;
  void set_title_text(const char* title);
  void set_title_alignment(lv_align_t align, int32_t x_offset = 8, int32_t y_offset = 0);
  void set_nav_action(std::size_t index, NavAction action);
  void set_keypad_nav_action(uint32_t keypad, NavAction action);
  void set_nav_actions(std::array<NavAction, 5> actions);
  void clear_nav_actions();
  bool trigger_nav_action(std::size_t index, lv_event_code_t event_code);
  void notify_nav_action_pressed(std::size_t index);
  void notify_nav_action_released(std::size_t index);
  void show_start_page();
  void show_keyboard_test_page();
  void show_lcd_test_page();
  void show_audio_test_page();
  void show_camera_test_page();
  void show_ir_send_test_page();
  void show_ir_receive_test_page();
  void show_imu_test_page();
  void show_power_info_page();
  void show_device_info_page();
  void show_perf_test_page();
  void show_test_result_page();
  void start_full_test_sequence();
  void show_single_test_page(model::AppPage page);
  void refresh_current_page();
  void complete_current_test();
  void complete_current_test(model::TestResult result);
  const char* current_test_name() const;
  std::size_t current_test_number() const;
  std::size_t test_count() const;
  const std::string& test_result_path() const;
  void set_back_request_handler(BackRequestHandler handler, void* user_data);
  void clear_back_request_handler(BackRequestHandler handler, void* user_data);
  void request_back_or_quit();
  void request_quit();

 protected:
  void publish_all_();
  void show_page_(model::AppPage page);

 private:
  model::AppModel model_{};
  model::TranslationService& translations_;
  model::TestSession test_session_{};
  std::size_t test_sequence_index_{0};
  std::string title_msgid_{};
  reactive::StringSubject<48> title_subject_;
  reactive::IntSubject title_alignment_subject_;
  reactive::IntSubject title_x_offset_subject_;
  reactive::IntSubject title_y_offset_subject_;
  reactive::IntSubject nav_actions_subject_{0};
  int32_t nav_actions_revision_{0};
  std::array<NavAction, 5> nav_actions_{};
  reactive::BoolSubject dark_mode_subject_;
  reactive::IntSubject language_subject_{0};
  reactive::IntSubject current_page_subject_;
  reactive::BoolSubject quit_requested_subject_;
  BackRequestHandler back_request_handler_{nullptr};
  void* back_request_user_data_{nullptr};
};

}  // namespace viewmodel
