/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

#include "base_widget.h"
#include "lvgl.h"
#include "theme.h"

namespace viewmodel {
class AppViewModel;
}

namespace view::widgets {

enum class PopupTone {
  DEFAULT,
  SUCCESS,
  WARNING,
  ERROR,
};

struct PopupConfig {
  int32_t width{236};
  int32_t height{44};
  lv_align_t align{LV_ALIGN_CENTER};
  int32_t offset_x{0};
  int32_t offset_y{4};
  int32_t radius{12};
  lv_opa_t bg_opa{LV_OPA_90};
  int32_t border_width{1};
  int32_t shadow_width{12};
  lv_opa_t shadow_opa{LV_OPA_20};
  int32_t pad_all{8};
  int32_t label_width{220};
  lv_text_align_t text_align{LV_TEXT_ALIGN_CENTER};
  lv_label_long_mode_t long_mode{LV_LABEL_LONG_DOT};
  PopupTone tone{PopupTone::DEFAULT};
  std::string message{};
  const lv_font_t* font{&lv_font_montserrat_14};
  bool clickable{false};
};

class Popup : public BaseWidgets {
 public:
  Popup(lv_obj_t* parent, PopupConfig config = {});
  Popup(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, PopupConfig config = {});
  ~Popup() override;

  static void show_transient(lv_obj_t* parent, PopupConfig config, uint32_t duration_ms);
  void build() override;
  void set_text(const char* text);
  void show();
  void hide();
  void close();
  bool visible() const;
  void show_for(uint32_t duration_ms);

 protected:
  void apply_theme(bool dark_mode) override;

 private:
  static void auto_close_cb(lv_timer_t* timer);
  lv_color_t border_color_(const view::ThemePalette& colors) const;

  static void transient_close_cb(lv_timer_t* timer);

  viewmodel::AppViewModel* app_view_model_{nullptr};
  PopupConfig config_{};
  lv_obj_t* label_{nullptr};
  lv_timer_t* auto_close_timer_{nullptr};
};

}  // namespace view::widgets
