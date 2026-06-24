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
#include "connectivity_service.h"
#include "dialog.h"
#include "lvgl.h"

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
  void show_config_dialog();
  bool dialog_visible() const;

 private:
  void open_session_();
  void poll_rx_();
  void append_log_(const char* prefix, const std::string& text);
  void refresh_log_label_();
  void update_input_label_();
  void send_input_();
  void show_status_(const std::string& message);
  void hide_config_dialog_();
  void apply_config_dialog_();
  bool append_dialog_char_(char ch);

  static void poll_timer_cb(lv_timer_t* timer);

  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* dialog_parent_{nullptr};
  lv_obj_t* root_{nullptr};
  lv_obj_t* log_view_{nullptr};
  lv_obj_t* log_label_{nullptr};
  lv_obj_t* input_label_{nullptr};
  lv_obj_t* status_label_{nullptr};
  std::unique_ptr<view::widgets::Dialog> dialog_{};
  lv_obj_t* baud_input_{nullptr};
  lv_timer_t* poll_timer_{nullptr};
  std::unique_ptr<platform::connectivity::UartDebugSession> session_{};
  std::vector<std::string> log_lines_{};
  std::string input_buffer_{};
  int baud_rate_{115200};
};

}  // namespace screen
