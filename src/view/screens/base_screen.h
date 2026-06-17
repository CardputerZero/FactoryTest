/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>

#include "app_viewmodel.h"
#include "lcd_test_viewmodel.h"
#include "lvgl.h"
#include "navbar.h"
#include "start_menu_viewmodel.h"
#include "titlebar.h"

namespace app {
class AssetManager;
}

namespace screen {

class BaseScreen {
 public:
  BaseScreen(viewmodel::AppViewModel& app_view_model,
             app::AssetManager& assets,
             viewmodel::LcdTestViewModel* lcd_view_model          = nullptr,
             viewmodel::StartMenuViewModel* start_menu_view_model = nullptr);
  virtual ~BaseScreen();

  BaseScreen(const BaseScreen&)            = delete;
  BaseScreen& operator=(const BaseScreen&) = delete;

  void init();
  lv_obj_t* root() const;

 protected:
  virtual void build_content(lv_obj_t* content) = 0;

  viewmodel::AppViewModel& app_view_model_ref_();
  app::AssetManager& assets_ref_();
  view::widgets::TitleBar* title_bar_ref_();

 private:
  viewmodel::AppViewModel& app_view_model_;
  app::AssetManager& assets_;
  viewmodel::LcdTestViewModel* lcd_view_model_{nullptr};
  viewmodel::StartMenuViewModel* start_menu_view_model_{nullptr};
  lv_obj_t* root_{nullptr};
  lv_obj_t* content_{nullptr};
  std::unique_ptr<view::widgets::TitleBar> title_bar_{};
  std::unique_ptr<view::widgets::NavBar> nav_bar_{};
};

}  // namespace screen
