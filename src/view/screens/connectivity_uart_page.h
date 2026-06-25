/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "uart_service.h"
#include "dialog.h"
#include "lvgl.h"
#include "popup.h"

namespace screen {

class UartConnectivityView {
 public:
  UartConnectivityView();
  ~UartConnectivityView();

  void build(lv_obj_t* parent,
             lv_obj_t* dialog_parent,
             viewmodel::AppViewModel& app_view_model,
             app::AssetManager& assets);
  bool handle_key(uint32_t key, const char* key_name);
  bool handle_key_release(uint32_t key, const char* key_name);
  bool handle_long_key(uint32_t key, const char* key_name);
  void show_config_dialog();
  bool dialog_visible() const;

 private:
  enum class HoldAction {
    NONE,
    CLEAR_BUFFER,
    BAUD_SETTINGS,
    SCREENSHOT,
    THEME_TOGGLE,
    EXIT_UART,
  };

  void open_session_();
  void poll_rx_();
  void append_log_(const char* prefix, const std::string& text);
  void refresh_log_label_();
  void update_input_label_();
  void send_input_();
  void reset_buffers_();
  void show_status_(const std::string& message);
  void hide_config_dialog_();
  void apply_config_dialog_();
  bool handle_dialog_key_(uint32_t key, const char* key_name);
  void step_baud_selection_(int delta);
  int selected_baud_rate_() const;
  uint32_t baud_index_for_rate_(int baud_rate) const;
  void show_hold_popup_(HoldAction action);
  void hide_hold_popup_();
  HoldAction hold_action_for_key_(uint32_t key) const;
  void begin_hold_action_(uint32_t key, HoldAction action);
  bool pending_hold_matches_(uint32_t key) const;
  bool append_input_char_(char ch);
  void apply_theme_(bool dark_mode);

  static void poll_timer_cb(lv_timer_t* timer);
  static void theme_observer(lv_observer_t* observer, lv_subject_t* subject);

  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* dialog_parent_{nullptr};
  lv_obj_t* root_{nullptr};
  lv_obj_t* log_view_{nullptr};
  lv_obj_t* log_label_{nullptr};
  lv_obj_t* input_card_{nullptr};
  lv_obj_t* input_label_{nullptr};
  lv_obj_t* status_label_{nullptr};
  std::unique_ptr<view::widgets::Dialog> dialog_{};
  std::unique_ptr<view::widgets::Popup> hold_popup_{};
  lv_obj_t* baud_dropdown_{nullptr};
  lv_timer_t* poll_timer_{nullptr};
  lv_observer_t* theme_observer_handle_{nullptr};
  std::unique_ptr<platform::connectivity::UartDebugSession> session_{};
  std::vector<std::string> log_lines_{};
  std::string input_buffer_{};
  HoldAction pending_hold_action_{HoldAction::NONE};
  uint32_t pending_hold_key_{0};
  bool pending_hold_consumed_{false};
  int baud_rate_{9600};
};

}  // namespace screen
