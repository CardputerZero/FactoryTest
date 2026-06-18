/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include "app_viewmodel.h"
#include "asset_manager.h"
#include "connectivity_test_viewmodel.h"
#include "lvgl.h"

namespace screen {

class LinkConnectivityView {
 public:
  explicit LinkConnectivityView(viewmodel::LinkConnectivityViewModel& view_model);
  ~LinkConnectivityView();
  void build(lv_obj_t* parent,
             lv_obj_t* dialog_parent,
             viewmodel::AppViewModel& app_view_model,
             app::AssetManager& assets);
  void restart();
  void show_config_dialog();
  bool handle_key(uint32_t key);
  bool dialog_visible() const;

 private:
  enum class DialogField {
    HOST,
    PORT,
  };

  void refresh_();
  void rebuild_();
  void hide_config_dialog_();
  void apply_config_dialog_();
  void set_active_dialog_field_(DialogField field);
  lv_obj_t* active_dialog_input_() const;
  bool append_dialog_char_(char ch);
  static void refresh_timer_cb(lv_timer_t* timer);
  static void dialog_apply_cb(lv_event_t* event);
  static void dialog_cancel_cb(lv_event_t* event);
  static void dialog_host_focus_cb(lv_event_t* event);
  static void dialog_port_focus_cb(lv_event_t* event);

  viewmodel::LinkConnectivityViewModel& view_model_;
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* dialog_parent_{nullptr};
  lv_obj_t* panel_{nullptr};
  lv_obj_t* dialog_{nullptr};
  lv_obj_t* host_input_{nullptr};
  lv_obj_t* port_input_{nullptr};
  DialogField active_dialog_field_{DialogField::HOST};
  bool panel_initialized_{false};
  lv_timer_t* refresh_timer_{nullptr};
};

}  // namespace screen
