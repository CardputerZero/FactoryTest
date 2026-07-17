/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "dialog.h"
#include "gpio_service.h"
#include "lvgl.h"

namespace screen {

class ExtIoConnectivityView {
 public:
  ExtIoConnectivityView() = default;
  ~ExtIoConnectivityView();

  ExtIoConnectivityView(const ExtIoConnectivityView&)            = delete;
  ExtIoConnectivityView& operator=(const ExtIoConnectivityView&) = delete;

  void build(lv_obj_t* parent,
             lv_obj_t* dialog_parent,
             viewmodel::AppViewModel& app_view_model,
             app::AssetManager& assets);
  bool handle_key(uint32_t key, const char* key_name);
  void show_config_dialog();
  bool dialog_visible() const;

 private:
  enum class IoFunction {
    OUTPUT,
    INPUT,
  };

  struct SwitchRow {
    const char* title{nullptr};
    const char* detail{nullptr};
    const char* icon{nullptr};
    platform::gpio::OutputLineConfig gpio{};
    bool supports_input{true};
    const char* brightness_path{nullptr};
    lv_obj_t* row{nullptr};
    lv_obj_t* icon_label{nullptr};
    lv_obj_t* title_label{nullptr};
    lv_obj_t* detail_label{nullptr};
    lv_obj_t* state_label{nullptr};
    lv_obj_t* led_obj{nullptr};
    lv_obj_t* switch_obj{nullptr};
    bool active{false};
    bool error{false};
    std::string error_message{};
  };

  void select_previous_();
  void select_next_();
  void set_selected_index_(std::size_t index);
  void toggle_selected_();
  void toggle_row_(std::size_t index);
  void release_output_lines_();
  void read_output_states_();
  void read_input_states_();
  void wait_for_gpio_slot_();
  void apply_theme_();
  void apply_row_style_(SwitchRow& row);
  void scroll_selected_to_view_();
  void hide_config_dialog_();
  void apply_config_dialog_();
  bool handle_dialog_key_(uint32_t key, const char* key_name);
  void step_function_selection_(int delta);
  uint32_t function_index_() const;
  IoFunction selected_function_() const;
  bool row_visible_(std::size_t index) const;
  void ensure_selected_visible_();
  void update_poll_timer_();

  static void row_clicked_cb(lv_event_t* event);
  static void input_poll_timer_cb(lv_timer_t* timer);
  static void theme_observer(lv_observer_t* observer, lv_subject_t* subject);

  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* dialog_parent_{nullptr};
  lv_obj_t* root_{nullptr};
  std::array<SwitchRow, 5> rows_{};
  std::unique_ptr<view::widgets::Dialog> dialog_{};
  lv_obj_t* function_dropdown_{nullptr};
  lv_timer_t* input_poll_timer_{nullptr};
  std::size_t selected_index_{0};
  IoFunction io_function_{IoFunction::OUTPUT};
  uint32_t last_gpio_action_at_{0};
  bool has_gpio_action_{false};
  lv_observer_t* theme_observer_handle_{nullptr};
};

}  // namespace screen
