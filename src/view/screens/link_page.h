/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <memory>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "connectivity_test_viewmodel.h"
#include "dialog.h"
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
  bool handle_key(uint32_t key, const char* key_name);
  bool dialog_visible() const;

 private:
  void refresh_();
  void rebuild_();
  void hide_config_dialog_();
  void apply_config_dialog_();
  bool append_dialog_char_(char ch);
  static void refresh_timer_cb(lv_timer_t* timer);
  static void dialog_host_focus_cb(lv_event_t* event);

  viewmodel::LinkConnectivityViewModel& view_model_;
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* dialog_parent_{nullptr};
  lv_obj_t* panel_{nullptr};
  std::unique_ptr<view::widgets::Dialog> dialog_{};
  lv_obj_t* host_input_{nullptr};
  bool panel_initialized_{false};
  lv_timer_t* refresh_timer_{nullptr};
};

}  // namespace screen
