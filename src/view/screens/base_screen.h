/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include <functional>
#include <memory>

#include "app_viewmodel.h"
#include "lvgl.h"
#include "navbar.h"
#include "dialog.h"
#include "titlebar.h"

namespace app {
class AssetManager;
}

namespace screen {

class BaseScreen {
 public:
  BaseScreen(viewmodel::AppViewModel& app_view_model,
             app::AssetManager& assets);
  virtual ~BaseScreen();

  BaseScreen(const BaseScreen&)            = delete;
  BaseScreen& operator=(const BaseScreen&) = delete;

  void init();
  lv_obj_t* root() const;

 protected:
  virtual void build_content(lv_obj_t* content) = 0;

  viewmodel::AppViewModel& app_view_model_ref_();
  app::AssetManager& assets_ref_();
  void set_nav_action_(uint32_t keypad,
                       const char* icon,
                       std::function<void()> action,
                       lv_event_code_t event_code           = LV_EVENT_CLICKED,
                       viewmodel::NavHoldTarget hold_target = viewmodel::NavHoldTarget::NONE,
                       bool force_enabled                   = false,
                       std::function<void()> press_action   = {},
                       std::function<void()> release_action = {});
  void set_default_test_nav_(bool show_complete = true);
  void show_test_result_dialog_();
  bool handle_test_result_dialog_key_(uint32_t key, const char* key_name = nullptr);
  view::widgets::TitleBar* title_bar_ref_();

 private:
  viewmodel::AppViewModel& app_view_model_;
  app::AssetManager& assets_;
  lv_obj_t* root_{nullptr};
  lv_obj_t* content_{nullptr};
  std::unique_ptr<view::widgets::TitleBar> title_bar_{};
  std::unique_ptr<view::widgets::NavBar> nav_bar_{};
  std::unique_ptr<view::widgets::Dialog> test_result_dialog_{};
};

}  // namespace screen
