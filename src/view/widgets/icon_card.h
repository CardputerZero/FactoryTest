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

class IconCard : public BaseWidgets {
 public:
  IconCard(lv_obj_t* parent,
           viewmodel::AppViewModel& view_model,
           const char* icon,
           const char* title,
           const char* value,
           const lv_font_t* icon_font,
           const lv_font_t* title_font,
           const lv_font_t* value_font,
           int32_t width,
           int32_t height,
           lv_label_long_mode_t value_long_mode = LV_LABEL_LONG_CLIP);

  void build() override;
  void set_icon(const char* icon);
  void set_value(const char* value);
  void set_value_long_mode(lv_label_long_mode_t mode);

 protected:
  void apply_theme(bool dark_mode) override;

 private:
  viewmodel::AppViewModel& view_model_;
  std::string icon_{};
  std::string title_{};
  std::string value_{};
  const lv_font_t* icon_font_{nullptr};
  const lv_font_t* title_font_{nullptr};
  const lv_font_t* value_font_{nullptr};
  int32_t width_{0};
  int32_t height_{0};
  lv_label_long_mode_t value_long_mode_{LV_LABEL_LONG_CLIP};
  lv_obj_t* icon_label_{nullptr};
  lv_obj_t* title_label_{nullptr};
  lv_obj_t* value_label_{nullptr};
};

}  // namespace view::widgets
