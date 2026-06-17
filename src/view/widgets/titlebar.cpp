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
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_14, 0);
  lv_obj_align(title_label_, title_align_, title_x_offset_, title_y_offset_);
  reactive::bind_theme(title_label_,
                       app_view_model_.dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  center_label_ = lv_label_create(core_obj_);
  lv_label_set_text(center_label_, "");
  lv_obj_set_style_text_font(center_label_, &lv_font_montserrat_14, 0);
  lv_obj_align(center_label_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(center_label_, LV_OBJ_FLAG_HIDDEN);
  reactive::bind_theme(center_label_,
                       app_view_model_.dark_mode_subject(),
                       reactive::ThemeRole::TEXT);
}

void TitleBar::set_title_alignment(lv_align_t align, int32_t x_offset, int32_t y_offset) {
  title_align_    = align;
  title_x_offset_ = x_offset;
  title_y_offset_ = y_offset;
  if (title_label_) {
    lv_obj_align(title_label_, title_align_, title_x_offset_, title_y_offset_);
  }
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

}  // namespace view::widgets
