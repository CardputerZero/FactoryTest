/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "base_screen.h"
#include "connectivity_test_viewmodel.h"
#include "keyboard_test_viewmodel.h"
#include "lcd_test_viewmodel.h"
#include "lvgl.h"
#include "perf_test_viewmodel.h"
#include "start_menu_viewmodel.h"

namespace app {

class ScreenManager {
 public:
  ScreenManager(viewmodel::AppViewModel& app_view_model,
                viewmodel::StartMenuViewModel& start_menu_view_model,
                viewmodel::KeyboardTestViewModel& keyboard_view_model,
                viewmodel::LcdTestViewModel& lcd_view_model,
                viewmodel::ConnectivityTestViewModel& connectivity_view_model,
                viewmodel::PerfTestViewModel& perf_view_model,
                AssetManager& assets);
  ~ScreenManager();

  ScreenManager(const ScreenManager&)            = delete;
  ScreenManager& operator=(const ScreenManager&) = delete;

  void start();
  void show_start_page();
  void show_keyboard_test_page();
  void show_lcd_test_page();
  void show_audio_test_page();
  void show_camera_test_page();
  void show_connectivity_test_page();
  void show_io_test_page(model::AppPage page);
  void show_ir_test_page();
  void show_imu_test_page();
  void show_power_info_page();
  void show_device_info_page();
  void show_perf_test_page();
  void show_perf_single_test_page(model::AppPage page);
  void show_test_result_page();
  void request_ftl_effect_page();
  void show_ftl_effect_page();
  void show_placeholder_page(model::AppPage page);
  void request_page(model::AppPage page);
  void flush_requested_page();
  lv_obj_t* current_screen() const;

 protected:
  void load_screen_(std::unique_ptr<screen::BaseScreen> next_screen);

 private:
  viewmodel::AppViewModel& app_view_model_;
  viewmodel::StartMenuViewModel& start_menu_view_model_;
  viewmodel::KeyboardTestViewModel& keyboard_view_model_;
  viewmodel::LcdTestViewModel& lcd_view_model_;
  viewmodel::ConnectivityTestViewModel& connectivity_view_model_;
  viewmodel::PerfTestViewModel& perf_view_model_;
  AssetManager& assets_;
  std::unique_ptr<screen::BaseScreen> current_screen_{};
  lv_observer_t* page_observer_{nullptr};
  lv_observer_t* ftl_page_observer_{nullptr};
  model::AppPage requested_page_{model::AppPage::START};
  model::AppPage loaded_page_{model::AppPage::START};
  bool has_loaded_page_{false};
  bool ftl_page_loaded_{false};
  bool page_switch_scheduled_{false};
  bool ftl_page_switch_scheduled_{false};
};

}  // namespace app
