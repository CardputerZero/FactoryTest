/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "popup.h"

#include <utility>

#include "app_viewmodel.h"
#include "theme.h"

namespace view::widgets {

Popup::Popup(lv_obj_t* parent, PopupConfig config)
    : BaseWidgets(parent),
      config_(std::move(config)) {}

Popup::Popup(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, PopupConfig config)
    : BaseWidgets(parent),
      app_view_model_(&app_view_model),
      config_(std::move(config)) {}

Popup::~Popup() {
  if (auto_close_timer_) {
    lv_timer_delete(auto_close_timer_);
    auto_close_timer_ = nullptr;
  }
}

void Popup::build() {
  if (core_obj_ || !parent_) {
    return;
  }

  core_obj_ = lv_obj_create(parent_);
  lv_obj_remove_style_all(core_obj_);
  lv_obj_set_size(core_obj_, config_.width, config_.height);
  lv_obj_align(core_obj_, config_.align, config_.offset_x, config_.offset_y);
  lv_obj_set_style_radius(core_obj_, config_.radius, 0);
  lv_obj_set_style_bg_opa(core_obj_, config_.bg_opa, 0);
  lv_obj_set_style_border_width(core_obj_, config_.border_width, 0);
  lv_obj_set_style_shadow_width(core_obj_, config_.shadow_width, 0);
  lv_obj_set_style_shadow_opa(core_obj_, config_.shadow_opa, 0);
  lv_obj_set_style_pad_all(core_obj_, config_.pad_all, 0);
  lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
  if (config_.clickable) {
    lv_obj_add_flag(core_obj_, LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_CLICKABLE);
  }

  label_ = lv_label_create(core_obj_);
  lv_label_set_text(label_, config_.message.c_str());
  lv_label_set_long_mode(label_, config_.long_mode);
  lv_obj_set_width(label_, config_.label_width);
  lv_obj_set_style_text_align(label_, config_.text_align, 0);
  lv_obj_set_style_text_font(label_, config_.font ? config_.font : &lv_font_montserrat_14, 0);
  lv_obj_center(label_);

  if (app_view_model_) {
    bind_theme_(app_view_model_->dark_mode_subject(), app_view_model_->is_dark_mode());
  } else {
    apply_theme_(true);
  }
  hide();
}

void Popup::show_transient(lv_obj_t* parent, PopupConfig config, uint32_t duration_ms) {
  auto* popup = new Popup(parent, std::move(config));
  popup->build();
  popup->show();
  auto* timer = lv_timer_create(transient_close_cb, duration_ms, popup);
  if (!timer) {
    delete popup;
    return;
  }
  lv_timer_set_repeat_count(timer, 1);
  lv_timer_set_auto_delete(timer, true);
}

void Popup::set_text(const char* text) {
  config_.message = text ? text : "";
  if (label_) {
    lv_label_set_text(label_, config_.message.c_str());
    lv_obj_center(label_);
  }
}

void Popup::show() {
  if (!core_obj_) {
    build();
  }
  if (!core_obj_) {
    return;
  }

  lv_obj_move_foreground(core_obj_);
  lv_obj_remove_flag(core_obj_, LV_OBJ_FLAG_HIDDEN);
}

void Popup::hide() {
  if (core_obj_ && lv_obj_is_valid(core_obj_)) {
    lv_obj_add_flag(core_obj_, LV_OBJ_FLAG_HIDDEN);
  }
}

void Popup::close() {
  if (auto_close_timer_) {
    lv_timer_delete(auto_close_timer_);
    auto_close_timer_ = nullptr;
  }
  if (core_obj_ && lv_obj_is_valid(core_obj_)) {
    lv_obj_delete(core_obj_);
  }
  core_obj_ = nullptr;
  label_    = nullptr;
}

bool Popup::visible() const {
  return core_obj_ && lv_obj_is_valid(core_obj_) &&
         !lv_obj_has_flag(core_obj_, LV_OBJ_FLAG_HIDDEN);
}

void Popup::show_for(uint32_t duration_ms) {
  show();
  if (!core_obj_) {
    return;
  }

  if (!auto_close_timer_) {
    auto_close_timer_ = lv_timer_create(auto_close_cb, duration_ms, this);
    if (!auto_close_timer_) {
      return;
    }
    lv_timer_set_auto_delete(auto_close_timer_, false);
  }
  lv_timer_set_period(auto_close_timer_, duration_ms);
  lv_timer_set_repeat_count(auto_close_timer_, 1);
  lv_timer_resume(auto_close_timer_);
  lv_timer_reset(auto_close_timer_);
}

void Popup::apply_theme(bool dark_mode) {
  if (!core_obj_) {
    return;
  }

  const auto colors = view::palette(dark_mode);
  lv_obj_set_style_bg_color(core_obj_, colors.button, 0);
  lv_obj_set_style_border_color(core_obj_, border_color_(colors), 0);
  if (label_) {
    lv_obj_set_style_text_color(label_, colors.text, 0);
  }
}

void Popup::auto_close_cb(lv_timer_t* timer) {
  auto* popup = static_cast<Popup*>(lv_timer_get_user_data(timer));
  if (popup) {
    popup->hide();
  }
}

void Popup::transient_close_cb(lv_timer_t* timer) {
  auto* popup = static_cast<Popup*>(lv_timer_get_user_data(timer));
  delete popup;
}

lv_color_t Popup::border_color_(const view::ThemePalette& colors) const {
  switch (config_.tone) {
    case PopupTone::SUCCESS:
      return colors.success;
    case PopupTone::WARNING:
      return colors.warning;
    case PopupTone::ERROR:
      return colors.error;
    case PopupTone::DEFAULT:
    default:
      return colors.border;
  }
}

}  // namespace view::widgets
