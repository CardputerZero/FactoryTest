/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "icon_button.h"

#include <algorithm>

#include "theme.h"

namespace view::widgets {
namespace {

constexpr int32_t K_HOLD_PROGRESS_MAX = 255;
constexpr int32_t K_FLASH_VALUE_MAX   = 255;
constexpr int32_t K_INDICATOR_WIDTH   = 2;
constexpr int32_t K_INDICATOR_HEIGHT  = 6;
constexpr int32_t K_LABEL_Y           = -3;
constexpr int32_t K_INDICATOR_Y       = 0;

}  // namespace

IconButton::IconButton(lv_obj_t* parent,
                       viewmodel::AppViewModel& view_model,
                       int32_t width,
                       int32_t height,
                       const char* text,
                       const lv_font_t* font,
                       lv_color_t light_color,
                       lv_color_t dark_color,
                       lv_event_cb_t click_cb,
                       void* user_data,
                       bool show_indicator)
    : BaseWidgets(parent),
      view_model_(view_model),
      width_(width),
      height_(height),
      text_(text ? text : ""),
      font_(font),
      light_color_(light_color),
      dark_color_(dark_color),
      click_cb_(click_cb),
      user_data_(user_data),
      show_indicator_(show_indicator) {}

IconButton::~IconButton() {
  // Animations target this widget as their variable, so they must not outlive it.
  lv_anim_delete(this, set_flash_value);
  lv_anim_delete(this, set_hold_progress_value);
}

void IconButton::build() {
  if (core_obj_) {
    return;
  }

  core_obj_ = lv_button_create(parent_);
  register_core_obj_();
  lv_obj_remove_style_all(core_obj_);
  lv_obj_set_size(core_obj_, width_, height_);
  lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(core_obj_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(core_obj_, 0, 0);
  lv_obj_set_style_shadow_width(core_obj_, 0, 0);
  lv_obj_set_style_pad_all(core_obj_, 0, 0);
  lv_obj_add_flag(core_obj_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  if (click_cb_) {
    lv_obj_add_event_cb(core_obj_,
                        click_cb_,
                        LV_EVENT_CLICKED,
                        user_data_ ? user_data_ : &view_model_);
  }

  label_ = lv_label_create(core_obj_);
  lv_label_set_text(label_, text_.c_str());
  if (font_) {
    lv_obj_set_style_text_font(label_, font_, 0);
  }
  lv_obj_align(label_, LV_ALIGN_CENTER, 0, K_LABEL_Y);

  if (show_indicator_) {
    indicator_ = lv_obj_create(core_obj_);
    lv_obj_remove_style_all(indicator_);
    lv_obj_set_size(indicator_, K_INDICATOR_WIDTH, K_INDICATOR_HEIGHT);
    lv_obj_set_style_radius(indicator_, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(indicator_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(indicator_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(indicator_, LV_ALIGN_BOTTOM_MID, 0, K_INDICATOR_Y);
  }

  bind_theme_(view_model_.dark_mode_subject(), view_model_.is_dark_mode());
}

void IconButton::set_text(const char* text) {
  text_ = text ? text : "";
  if (label_) {
    lv_label_set_text(label_, text_.c_str());
    lv_obj_align(label_, LV_ALIGN_CENTER, 0, K_LABEL_Y);
  }
  apply_indicator_style_();
}

void IconButton::set_font(const lv_font_t* font) {
  font_ = font;
  if (label_ && font_) {
    lv_obj_set_style_text_font(label_, font_, 0);
    lv_obj_align(label_, LV_ALIGN_CENTER, 0, K_LABEL_Y);
  }
}

void IconButton::set_color(lv_color_t light_color, lv_color_t dark_color) {
  light_color_ = light_color;
  dark_color_  = dark_color;
  apply_text_color_();
}

void IconButton::flash(lv_color_t light_color, lv_color_t dark_color, uint32_t duration_ms) {
  if (!label_) {
    return;
  }

  lv_anim_delete(this, set_flash_value);
  flash_light_color_ = light_color;
  flash_dark_color_  = dark_color;
  flash_value_       = 0;
  flash_active_      = true;
  apply_text_color_();

  lv_anim_t animation{};
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, this);
  lv_anim_set_values(&animation, 0, K_FLASH_VALUE_MAX);
  lv_anim_set_time(&animation, duration_ms);
  lv_anim_set_path_cb(&animation, lv_anim_path_linear);
  lv_anim_set_exec_cb(&animation, set_flash_value);
  lv_anim_start(&animation);
}

void IconButton::stop_flash() {
  lv_anim_delete(this, set_flash_value);
  flash_active_ = false;
  flash_value_  = 0;
  apply_text_color_();
}

void IconButton::set_flash_value(void* obj, int32_t value) {
  auto* button = static_cast<IconButton*>(obj);
  if (!button) {
    return;
  }

  button->flash_value_ = std::clamp(value, 0, K_FLASH_VALUE_MAX);
  button->apply_text_color_();
}

void IconButton::set_enabled(bool enabled) {
  enabled_ = enabled;
  if (!core_obj_) {
    return;
  }

  if (enabled_) {
    lv_obj_add_flag(core_obj_, LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_CLICKABLE);
    reset_hold_progress();
    stop_flash();
  }
  apply_theme_(view_model_.is_dark_mode());
}

void IconButton::set_hold_target_color(lv_color_t color) {
  hold_target_color_     = color;
  has_hold_target_color_ = true;
  apply_text_color_();
}

void IconButton::clear_hold_target_color() {
  has_hold_target_color_ = false;
  reset_hold_progress();
}

void IconButton::start_hold_progress(uint32_t duration_ms) {
  if (!label_ || !enabled_ || !has_hold_target_color_) {
    return;
  }

  lv_anim_delete(this, set_hold_progress_value);
  hold_progress_active_ = true;
  hold_progress_value_  = 0;
  apply_text_color_();

  lv_anim_t animation{};
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, this);
  lv_anim_set_values(&animation, 0, K_HOLD_PROGRESS_MAX);
  lv_anim_set_time(&animation, duration_ms);
  lv_anim_set_exec_cb(&animation, set_hold_progress_value);
  lv_anim_start(&animation);
}

void IconButton::reset_hold_progress() {
  lv_anim_delete(this, set_hold_progress_value);
  hold_progress_active_ = false;
  hold_progress_value_  = 0;
  apply_text_color_();
}

void IconButton::set_hold_progress_value(void* obj, int32_t value) {
  auto* button = static_cast<IconButton*>(obj);
  if (!button) {
    return;
  }

  button->hold_progress_value_ = std::clamp(value, 0, K_HOLD_PROGRESS_MAX);
  button->apply_text_color_();
}

void IconButton::apply_theme(bool dark_mode) {
  LV_UNUSED(dark_mode);

  if (!core_obj_ || !label_) {
    return;
  }

  lv_obj_set_style_bg_opa(core_obj_, LV_OPA_TRANSP, 0);
  apply_text_color_();
  apply_indicator_style_();
}

lv_color_t IconButton::base_text_color_() const {
  return view_model_.is_dark_mode() ? dark_color_ : light_color_;
}

lv_color_t IconButton::disabled_text_color_() const {
  return view::palette(view_model_.is_dark_mode()).text_disabled;
}

lv_color_t IconButton::flash_peak_color_() const {
  return view_model_.is_dark_mode() ? flash_dark_color_ : flash_light_color_;
}

// 0 at both ends of the animation and 255 in the middle, so one flash leaves the
// label at its base color again.
uint8_t IconButton::flash_blend_amount_() const {
  const auto value = std::clamp(flash_value_, 0, K_FLASH_VALUE_MAX);
  const auto peak  = value <= K_FLASH_VALUE_MAX / 2 ? value * 2 : (K_FLASH_VALUE_MAX - value) * 2;
  return static_cast<uint8_t>(std::clamp(peak, 0, K_FLASH_VALUE_MAX));
}

void IconButton::apply_text_color_() {
  if (!label_) {
    return;
  }

  if (!enabled_) {
    lv_obj_set_style_text_color(label_, disabled_text_color_(), 0);
    apply_indicator_style_();
    return;
  }

  auto color = base_text_color_();
  if (hold_progress_active_ && has_hold_target_color_) {
    color = lv_color_mix(
        hold_target_color_,
        color,
        static_cast<uint8_t>(std::clamp(hold_progress_value_, 0, K_HOLD_PROGRESS_MAX)));
  }
  if (flash_active_) {
    color = lv_color_mix(flash_peak_color_(), color, flash_blend_amount_());
  }
  lv_obj_set_style_text_color(label_, color, 0);
  apply_indicator_style_();
}

void IconButton::apply_indicator_style_() {
  if (!indicator_) {
    return;
  }

  const bool show_indicator = enabled_ && !text_.empty();
  lv_obj_set_style_bg_opa(indicator_, show_indicator ? LV_OPA_70 : LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(indicator_, base_text_color_(), 0);
  lv_obj_align(indicator_, LV_ALIGN_BOTTOM_MID, 0, K_INDICATOR_Y);
}

}  // namespace view::widgets
