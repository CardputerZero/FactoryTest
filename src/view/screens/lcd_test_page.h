/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstdint>

#include "base_screen.h"
#include "lcd_test_viewmodel.h"

namespace screen {

class LcdTestPage : public BaseScreen {
 public:
  LcdTestPage(viewmodel::AppViewModel& app_view_model,
              viewmodel::LcdTestViewModel& lcd_view_model,
              app::AssetManager& assets);
  ~LcdTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;
  void apply_color_index_(int32_t index);
  void apply_cross_hatch_(bool dark_background);
  void set_cross_hatch_visible_(bool visible);
  void apply_brightness_active_(bool active);
  void apply_brightness_percent_(int32_t percent);
  void apply_brightness_theme_(bool dark_mode);
  void update_nav_actions_();
  void advance_color_step_();
  void show_tile_(std::size_t index, bool animate);
  void switch_to_next_tile_();
  void sync_active_tile_();
  void schedule_brightness_commit_();
  void begin_brightness_transition_();
  void step_brightness_transition_();
  static void color_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void brightness_active_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void brightness_percent_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void theme_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void brightness_commit_timer_cb(lv_timer_t* timer);
  static void brightness_smooth_timer_cb(lv_timer_t* timer);
  static void tile_scroll_end_cb(lv_event_t* event);
  static void key_listener(uint32_t key, const char* key_name, void* user_data);

 private:
  static constexpr std::size_t K_COLOR_TILE_INDEX      = 0;
  static constexpr std::size_t K_BRIGHTNESS_TILE_INDEX = 1;
  static constexpr std::size_t K_TILE_COUNT            = 2;

  viewmodel::LcdTestViewModel& lcd_view_model_;
  lv_obj_t* tileview_{nullptr};
  std::array<lv_obj_t*, K_TILE_COUNT> tiles_{};
  lv_obj_t* prompt_{nullptr};
  lv_obj_t* color_layer_{nullptr};
  lv_obj_t* color_label_{nullptr};
  lv_obj_t* cross_hatch_rect_{nullptr};
  lv_obj_t* cross_hatch_diag_a_{nullptr};
  lv_obj_t* cross_hatch_diag_b_{nullptr};
  std::array<lv_obj_t*, 4> cross_hatch_corners_{};
  lv_obj_t* brightness_group_{nullptr};
  lv_obj_t* brightness_label_{nullptr};
  lv_obj_t* brightness_slider_{nullptr};
  lv_observer_t* color_observer_handle_{nullptr};
  lv_observer_t* brightness_active_observer_handle_{nullptr};
  lv_observer_t* brightness_percent_observer_handle_{nullptr};
  lv_observer_t* theme_observer_handle_{nullptr};
  lv_timer_t* brightness_commit_timer_{nullptr};
  lv_timer_t* brightness_smooth_timer_{nullptr};
  int32_t requested_brightness_percent_{model::LcdTestModel::K_INITIAL_BRIGHTNESS_PERCENT};
  int32_t current_color_index_{0};
  int32_t hardware_brightness_percent_{model::LcdTestModel::K_INITIAL_BRIGHTNESS_PERCENT};
  int32_t target_brightness_percent_{model::LcdTestModel::K_INITIAL_BRIGHTNESS_PERCENT};
  std::size_t active_tile_index_{K_COLOR_TILE_INDEX};
  bool hardware_brightness_loaded_{false};
};

}  // namespace screen
