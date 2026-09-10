/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "baking_pi_service.h"
#include "base_screen.h"

namespace screen {

class FireInThePiPage : public BaseScreen {
 public:
  FireInThePiPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~FireInThePiPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  enum class ChartMetric : std::size_t {
    SOC_TEMPERATURE = 0,
    BATTERY_TEMPERATURE,
    VOLTAGE,
    CURRENT,
    CPU_USAGE,
    MEMORY_USAGE,
    STORAGE_USAGE,
    STORAGE_READ,
    STORAGE_WRITE,
  };

  void start_cooking_();
  void show_step_(std::size_t index, bool animate);
  void show_chart_();
  void hide_chart_();
  void change_chart_(int direction);
  void update_nav_();
  void update_runtime_ui_();
  void refresh_chart_();
  static void step_timer_cb(lv_timer_t* timer);
  static void ui_timer_cb(lv_timer_t* timer);
  static void key_listener(uint32_t key, const char* key_name, void* user_data);

  lv_obj_t* cooking_layer_{nullptr};
  lv_obj_t* status_label_{nullptr};
  lv_obj_t* cycle_label_{nullptr};
  lv_obj_t* chart_{nullptr};
  lv_obj_t* chart_value_label_{nullptr};
  lv_chart_series_t* chart_series_{nullptr};
  lv_timer_t* step_timer_{nullptr};
  lv_timer_t* ui_timer_{nullptr};
  std::size_t current_step_{0};
  ChartMetric chart_metric_{ChartMetric::SOC_TEMPERATURE};
  bool cooking_{false};
  bool chart_visible_{false};
  std::unique_ptr<platform::baking::BakingPiService> baking_service_{};
};

}  // namespace screen
