/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "connectivity_test_viewmodel.h"
#include "lvgl.h"

namespace screen {

class WifiConnectivityView {
 public:
  explicit WifiConnectivityView(viewmodel::WifiConnectivityViewModel& view_model);
  ~WifiConnectivityView();
  void build(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);

 private:
  void refresh_();
  static void refresh_timer_cb(lv_timer_t* timer);

  viewmodel::WifiConnectivityViewModel& view_model_;
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* panel_{nullptr};
  bool panel_initialized_{false};
  lv_timer_t* refresh_timer_{nullptr};
};

}  // namespace screen
