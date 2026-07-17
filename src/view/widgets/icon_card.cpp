/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "icon_card.h"

#include "theme.h"

namespace view::widgets {
namespace {

constexpr int32_t K_ICON_WIDTH = 22;
constexpr int32_t K_TITLE_X    = 30;
constexpr int32_t K_VALUE_PAD  = 8;

int32_t default_title_width(int32_t card_width) { return card_width >= 220 ? 74 : 56; }

}  // namespace

IconCard::IconCard(lv_obj_t* parent,
                   viewmodel::AppViewModel& view_model,
                   const char* icon,
                   const char* title,
                   const char* value,
                   const lv_font_t* icon_font,
                   const lv_font_t* title_font,
                   const lv_font_t* value_font,
                   int32_t width,
                   int32_t height,
                   lv_label_long_mode_t value_long_mode,
                   int32_t title_width)
    : BaseWidgets(parent),
      view_model_(view_model),
      icon_(icon ? icon : ""),
      title_(title ? title : ""),
      value_(value ? value : ""),
      icon_font_(icon_font),
      title_font_(title_font),
      value_font_(value_font),
      width_(width),
      height_(height),
      title_width_(title_width > 0 ? title_width : default_title_width(width)),
      value_long_mode_(value_long_mode) {}

void IconCard::build() {
  if (core_obj_) {
    return;
  }

  core_obj_ = lv_obj_create(parent_);
  register_core_obj_();
  lv_obj_remove_style_all(core_obj_);
  lv_obj_set_size(core_obj_, width_, height_);
  lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_CLICKABLE);

  icon_label_ = lv_label_create(core_obj_);
  lv_label_set_text(icon_label_, icon_.c_str());
  lv_obj_set_width(icon_label_, K_ICON_WIDTH);
  lv_obj_set_style_text_align(icon_label_, LV_TEXT_ALIGN_CENTER, 0);
  if (icon_font_) {
    lv_obj_set_style_text_font(icon_label_, icon_font_, 0);
  }
  lv_obj_align(icon_label_, LV_ALIGN_LEFT_MID, 4, 0);

  title_label_ = lv_label_create(core_obj_);
  lv_label_set_text(title_label_, title_.c_str());
  lv_label_set_long_mode(title_label_, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(title_label_, title_width_);
  if (title_font_) {
    lv_obj_set_style_text_font(title_label_, title_font_, 0);
  }
  lv_obj_align(title_label_, LV_ALIGN_LEFT_MID, K_TITLE_X, 0);

  value_label_ = lv_label_create(core_obj_);
  lv_label_set_text(value_label_, value_.c_str());
  lv_label_set_long_mode(value_label_, value_long_mode_);
  lv_obj_set_width(value_label_, width_ - K_TITLE_X - title_width_ - K_VALUE_PAD);
  lv_obj_set_style_text_align(value_label_, LV_TEXT_ALIGN_RIGHT, 0);
  if (value_font_) {
    lv_obj_set_style_text_font(value_label_, value_font_, 0);
  }
  lv_obj_align(value_label_, LV_ALIGN_RIGHT_MID, -K_VALUE_PAD, 0);

  bind_theme_(view_model_.dark_mode_subject(), view_model_.is_dark_mode());
}

void IconCard::set_icon(const char* icon) {
  icon_ = icon ? icon : "";
  if (icon_label_) {
    lv_label_set_text(icon_label_, icon_.c_str());
  }
}

void IconCard::set_value(const char* value) {
  value_ = value ? value : "";
  if (value_label_) {
    lv_label_set_text(value_label_, value_.c_str());
  }
}

void IconCard::set_value_long_mode(lv_label_long_mode_t mode) {
  value_long_mode_ = mode;
  if (value_label_) {
    lv_label_set_long_mode(value_label_, value_long_mode_);
  }
}

void IconCard::apply_theme(bool dark_mode) {
  if (!core_obj_) {
    return;
  }

  const auto colors = view::palette(dark_mode);
  lv_obj_set_style_radius(core_obj_, 6, 0);
  lv_obj_set_style_border_width(core_obj_, 1, 0);
  lv_obj_set_style_border_color(core_obj_, colors.border, 0);
  lv_obj_set_style_outline_width(core_obj_, 1, 0);
  lv_obj_set_style_outline_pad(core_obj_, 0, 0);
  lv_obj_set_style_outline_opa(core_obj_, LV_OPA_10, 0);
  lv_obj_set_style_outline_color(core_obj_, colors.border, 0);
  lv_obj_set_style_bg_opa(core_obj_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(core_obj_, colors.button, 0);

  if (icon_label_) {
    lv_obj_set_style_text_color(icon_label_, colors.text, 0);
  }
  if (title_label_) {
    lv_obj_set_style_text_color(title_label_, colors.text, 0);
  }
  if (value_label_) {
    lv_obj_set_style_text_color(value_label_, colors.text, 0);
  }
}

}  // namespace view::widgets
