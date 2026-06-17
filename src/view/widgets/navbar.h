/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <memory>

#include "app_viewmodel.h"
#include "base_widget.h"
#include "icon_button.h"
#include "lcd_test_viewmodel.h"
#include "start_menu_viewmodel.h"

namespace app {
class AssetManager;
}

namespace view::widgets {

class NavBar : public BaseWidgets {
 public:
  NavBar(lv_obj_t* parent,
         viewmodel::AppViewModel& app_view_model,
         app::AssetManager& assets,
         viewmodel::LcdTestViewModel* lcd_view_model          = nullptr,
         viewmodel::StartMenuViewModel* start_menu_view_model = nullptr);
  ~NavBar() override;

  void build() override;

 protected:
  void create_icon_buttons_();
  void update_icon_buttons_();
  void schedule_icon_update_();
  bool can_trigger_lcd_color_();
  bool can_trigger_lcd_brightness_();
  bool can_complete_lcd_test_();
  bool can_start_exit_() const;
  void mark_start_exit_debounce_();
  void show_start_exit_overlay_();
  void hide_start_exit_overlay_();
  void reset_start_exit_hold_();
  static void request_back_or_quit_cb(lv_event_t* event);
  static void start_exit_press_cb(lv_event_t* event);
  static void start_exit_confirm_cb(lv_event_t* event);
  static void start_exit_release_cb(lv_event_t* event);
  static void complete_test_cb(lv_event_t* event);
  static void complete_lcd_test_cb(lv_event_t* event);
  static void confirm_start_menu_cb(lv_event_t* event);
  static void advance_lcd_color_cb(lv_event_t* event);
  static void decrease_lcd_brightness_cb(lv_event_t* event);
  static void increase_lcd_brightness_cb(lv_event_t* event);
  static void start_hold_progress_cb(lv_event_t* event);
  static void reset_hold_progress_cb(lv_event_t* event);
  static void update_icons_cb(lv_observer_t* observer, lv_subject_t* subject);
  static void update_icons_async_cb(void* user_data);

 private:
  viewmodel::AppViewModel& app_view_model_;
  app::AssetManager& assets_;
  viewmodel::LcdTestViewModel* lcd_view_model_{nullptr};
  viewmodel::StartMenuViewModel* start_menu_view_model_{nullptr};
  std::array<std::unique_ptr<IconButton>, 5> icon_buttons_{};
  lv_font_t* icon_font_{nullptr};
  lv_observer_t* page_observer_{nullptr};
  lv_observer_t* app_theme_observer_{nullptr};
  lv_observer_t* brightness_observer_{nullptr};
  bool icon_update_scheduled_{false};
  uint32_t last_lcd_color_trigger_at_{0};
  uint32_t last_lcd_brightness_trigger_at_{0};
  uint32_t last_back_to_start_at_{0};
  bool has_lcd_color_trigger_{false};
  bool has_lcd_brightness_trigger_{false};
  bool has_back_to_start_debounce_{false};
  bool start_exit_hold_active_{false};
  lv_obj_t* start_exit_overlay_{nullptr};
  lv_obj_t* start_exit_overlay_label_{nullptr};
};

}  // namespace view::widgets
