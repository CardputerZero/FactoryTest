/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <vector>

#include "icon_card.h"
#include "app_viewmodel.h"
#include "asset_manager.h"
#include "connectivity_test_viewmodel.h"
#include "lvgl.h"

namespace screen {

class SpiConnectivityView {
 public:
  explicit SpiConnectivityView(viewmodel::SpiConnectivityViewModel& view_model);
  ~SpiConnectivityView();
  void build(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);

 private:
  void refresh_();
  static void refresh_timer_cb(lv_timer_t* timer);

  viewmodel::SpiConnectivityViewModel& view_model_;
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* list_{nullptr};
  std::vector<std::unique_ptr<view::widgets::IconCard>> cards_{};
  lv_timer_t* refresh_timer_{nullptr};
};

}  // namespace screen
