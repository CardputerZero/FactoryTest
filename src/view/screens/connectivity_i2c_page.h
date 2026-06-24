/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <memory>
#include <string>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "connectivity_test_viewmodel.h"
#include "lvgl.h"
#include "popup.h"

namespace screen {

class I2cConnectivityView {
 public:
  explicit I2cConnectivityView(viewmodel::I2cConnectivityViewModel& view_model);
  ~I2cConnectivityView();
  void build(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);

 private:
  void build_static_content_();
  void update_content_();
  void update_scanning_popup_();
  void refresh_();
  static void refresh_timer_cb(lv_timer_t* timer);

  viewmodel::I2cConnectivityViewModel& view_model_;
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* panel_{nullptr};
  lv_obj_t* card_{nullptr};
  lv_obj_t* grid_{nullptr};
  std::unique_ptr<view::widgets::Popup> scanning_popup_{};
  std::array<lv_obj_t*, 128> cell_labels_{};
  std::array<model::ConnectivityI2cAddressState, 128> cell_states_{};
  bool panel_initialized_{false};
  lv_timer_t* refresh_timer_{nullptr};
};

}  // namespace screen
