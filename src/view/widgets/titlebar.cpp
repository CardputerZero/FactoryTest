/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "titlebar.h"

#include "bindings.h"
#include "theme.h"

namespace view::widgets {

TitleBar::TitleBar(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model)
    : BaseWidgets(parent),
      app_view_model_(app_view_model) {}

TitleBar::~TitleBar() {
  if (title_align_observer_) {
    lv_observer_remove(title_align_observer_);
    title_align_observer_ = nullptr;
  }
  if (title_x_observer_) {
    lv_observer_remove(title_x_observer_);
    title_x_observer_ = nullptr;
  }
  if (title_y_observer_) {
    lv_observer_remove(title_y_observer_);
    title_y_observer_ = nullptr;
  }
}

void TitleBar::build() {
  if (core_obj_) {
    return;
  }

  core_obj_ = lv_obj_create(parent_);
  lv_obj_remove_style_all(core_obj_);
  lv_obj_set_size(core_obj_, LV_PCT(100), K_TITLE_BAR_HEIGHT);
  lv_obj_align(core_obj_, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(core_obj_, app_view_model_.dark_mode_subject(), reactive::ThemeRole::BAR);

  title_label_ = lv_label_create(core_obj_);
  lv_label_bind_text(title_label_, app_view_model_.title_subject(), nullptr);
  lv_label_set_long_mode(title_label_, LV_LABEL_LONG_DOT);
  lv_obj_set_width(title_label_, 190);
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_14, 0);
  reactive::bind_theme(title_label_,
                       app_view_model_.dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  center_label_ = lv_label_create(core_obj_);
  lv_label_set_text(center_label_, "");
  lv_label_set_long_mode(center_label_, LV_LABEL_LONG_DOT);
  lv_obj_set_width(center_label_, 120);
  lv_obj_set_style_text_align(center_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(center_label_, &lv_font_montserrat_14, 0);
  lv_obj_align(center_label_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(center_label_, LV_OBJ_FLAG_HIDDEN);
  reactive::bind_theme(center_label_,
                       app_view_model_.dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  title_align_observer_ = reactive::observe_obj(core_obj_,
                                                app_view_model_.title_alignment_subject(),
                                                title_layout_observer,
                                                this);
  title_x_observer_     = reactive::observe_obj(core_obj_,
                                                app_view_model_.title_x_offset_subject(),
                                                title_layout_observer,
                                                this);
  title_y_observer_     = reactive::observe_obj(core_obj_,
                                                app_view_model_.title_y_offset_subject(),
                                                title_layout_observer,
                                                this);
  apply_title_alignment_();
}

void TitleBar::set_title(const char* text) { app_view_model_.set_title_text(text); }

void TitleBar::set_title_alignment(lv_align_t align, int32_t x_offset, int32_t y_offset) {
  app_view_model_.set_title_alignment(align, x_offset, y_offset);
}

void TitleBar::set_center_text(const char* text) {
  if (!center_label_) {
    return;
  }

  lv_label_set_text(center_label_, text ? text : "");
  if (text && text[0] != '\0') {
    lv_obj_remove_flag(center_label_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(center_label_, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_align(center_label_, LV_ALIGN_CENTER, 0, 0);
}

void TitleBar::apply_title_alignment_() {
  title_align_ =
      static_cast<lv_align_t>(lv_subject_get_int(app_view_model_.title_alignment_subject()));
  title_x_offset_ = lv_subject_get_int(app_view_model_.title_x_offset_subject());
  title_y_offset_ = lv_subject_get_int(app_view_model_.title_y_offset_subject());
  if (title_label_) {
    lv_obj_align(title_label_, title_align_, title_x_offset_, title_y_offset_);
  }
}

void TitleBar::title_layout_observer(lv_observer_t* observer, lv_subject_t* subject) {
  LV_UNUSED(subject);

  auto* title_bar = static_cast<TitleBar*>(lv_observer_get_user_data(observer));
  if (title_bar) {
    title_bar->apply_title_alignment_();
  }
}

}  // namespace view::widgets
