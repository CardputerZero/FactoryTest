/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

#include "app_viewmodel.h"
#include "base_widget.h"

namespace view::widgets {

class IconButton : public BaseWidgets {
 public:
  IconButton(lv_obj_t* parent,
             viewmodel::AppViewModel& view_model,
             int32_t width,
             int32_t height,
             const char* text,
             const lv_font_t* font,
             lv_color_t light_color,
             lv_color_t dark_color,
             lv_event_cb_t click_cb = nullptr,
             void* user_data        = nullptr,
             bool show_indicator    = false);

  void build() override;
  void set_text(const char* text);
  void set_font(const lv_font_t* font);
  void set_color(lv_color_t light_color, lv_color_t dark_color);
  void set_enabled(bool enabled);
  void set_hold_target_color(lv_color_t color);
  void clear_hold_target_color();
  void start_hold_progress(uint32_t duration_ms);
  void reset_hold_progress();

 protected:
  void apply_theme(bool dark_mode) override;

 private:
  static void set_hold_progress_value(void* obj, int32_t value);

  lv_color_t base_text_color_() const;
  lv_color_t disabled_text_color_() const;
  void apply_text_color_();
  void apply_indicator_style_();

  viewmodel::AppViewModel& view_model_;
  int32_t width_{0};
  int32_t height_{0};
  std::string text_{};
  const lv_font_t* font_{nullptr};
  lv_color_t light_color_{};
  lv_color_t dark_color_{};
  lv_event_cb_t click_cb_{nullptr};
  void* user_data_{nullptr};
  bool show_indicator_{false};
  lv_obj_t* label_{nullptr};
  lv_obj_t* indicator_{nullptr};
  lv_color_t hold_target_color_{};
  int32_t hold_progress_value_{0};
  bool enabled_{true};
  bool has_hold_target_color_{false};
  bool hold_progress_active_{false};
};

}  // namespace view::widgets
