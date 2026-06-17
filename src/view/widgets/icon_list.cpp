/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "icon_list.h"

#include <algorithm>

#include "theme.h"
#include "ui_const.h"

namespace view::widgets {
namespace {

constexpr int32_t K_ROW_HEIGHT    = 22;
constexpr int32_t K_ICON_WIDTH    = 20;
constexpr int32_t K_TEXT_OFFSET_X = 26;
constexpr int32_t K_ROW_RADIUS    = 4;

}  // namespace

IconList::IconList(lv_obj_t* parent,
                   viewmodel::AppViewModel& view_model,
                   const std::vector<Item>& items,
                   const lv_font_t* text_font,
                   const lv_font_t* icon_font,
                   int32_t width,
                   int32_t height,
                   ItemClickedCallback clicked_callback,
                   void* user_data)
    : BaseWidgets(parent),
      view_model_(view_model),
      items_(items),
      text_font_(text_font),
      icon_font_(icon_font),
      width_(width),
      height_(height),
      clicked_callback_(clicked_callback),
      user_data_(user_data) {}

void IconList::build() {
  if (core_obj_) {
    return;
  }

  core_obj_ = lv_obj_create(parent_);
  lv_obj_remove_style_all(core_obj_);
  lv_obj_set_size(core_obj_, width_, height_);
  lv_obj_set_flex_flow(core_obj_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(core_obj_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(core_obj_, 3, 0);
  lv_obj_set_style_pad_top(core_obj_, 0, 0);
  lv_obj_set_style_pad_bottom(core_obj_, 0, 0);
  lv_obj_set_scroll_dir(core_obj_, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(core_obj_, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_center(core_obj_);

  rows_.clear();
  rows_.reserve(items_.size());
  for (std::size_t i = 0; i < items_.size(); ++i) {
    Row row{};
    row.index  = i;
    row.button = lv_button_create(core_obj_);
    lv_obj_remove_style_all(row.button);
    lv_obj_set_size(row.button, width_, K_ROW_HEIGHT);
    lv_obj_set_style_radius(row.button, K_ROW_RADIUS, 0);
    lv_obj_set_style_pad_hor(row.button, 8, 0);
    lv_obj_clear_flag(row.button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row.button, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_clear_flag(row.button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(row.button, item_clicked_cb, LV_EVENT_CLICKED, this);

    row.icon_label = lv_label_create(row.button);
    lv_label_set_text(row.icon_label, items_[i].icon ? items_[i].icon : "");
    lv_obj_set_width(row.icon_label, K_ICON_WIDTH);
    lv_obj_set_style_text_align(row.icon_label, LV_TEXT_ALIGN_CENTER, 0);
    if (icon_font_) {
      lv_obj_set_style_text_font(row.icon_label, icon_font_, 0);
    }
    lv_obj_align(row.icon_label, LV_ALIGN_LEFT_MID, 0, 0);

    row.text_label = lv_label_create(row.button);
    lv_label_set_text(row.text_label, items_[i].title ? items_[i].title : "");
    lv_label_set_long_mode(row.text_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(row.text_label, width_ - K_TEXT_OFFSET_X - 14);
    if (text_font_) {
      lv_obj_set_style_text_font(row.text_label, text_font_, 0);
    }
    lv_obj_align(row.text_label, LV_ALIGN_LEFT_MID, K_TEXT_OFFSET_X, 0);

    rows_.push_back(row);
  }

  bind_theme_(view_model_.dark_mode_subject(), view_model_.is_dark_mode());
  scroll_selected_to_view_();
}

void IconList::set_selected_index(std::size_t index) {
  if (items_.empty()) {
    selected_index_ = 0;
    return;
  }

  selected_index_ = std::min(index, items_.size() - 1);
  apply_theme_(view_model_.is_dark_mode());
  scroll_selected_to_view_();
}

void IconList::set_focused(bool focused) {
  if (focused_ == focused) {
    return;
  }

  focused_ = focused;
  apply_theme_(view_model_.is_dark_mode());
}

bool IconList::is_focused() const { return focused_; }

void IconList::apply_theme(bool dark_mode) {
  if (!core_obj_) {
    return;
  }

  const auto colors = view::palette(dark_mode);
  lv_obj_set_style_bg_opa(core_obj_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(core_obj_, colors.surface, 0);

  for (auto& row : rows_) {
    apply_row_style_(row, dark_mode);
  }
}

void IconList::apply_row_style_(Row& row, bool dark_mode) {
  if (!row.button || !row.text_label || !row.icon_label) {
    return;
  }

  const bool selected    = row.index == selected_index_;
  const auto colors      = view::palette(dark_mode);
  const auto normal_bg   = colors.surface;
  const auto selected_bg = lv_color_mix(colors.primary, colors.surface, dark_mode ? 44 : 26);
  const auto unfocused_selected_bg =
      lv_color_mix(colors.primary, colors.surface, dark_mode ? 30 : 18);
  const auto normal_text             = colors.text_muted;
  const auto selected_text           = colors.primary;
  const auto unfocused_selected_text = colors.primary_pressed;

  lv_obj_set_style_bg_opa(row.button, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(row.button,
                            selected ? (focused_ ? selected_bg : unfocused_selected_bg) : normal_bg,
                            0);
  lv_obj_set_style_border_width(row.button, selected && focused_ ? 1 : 0, 0);
  lv_obj_set_style_border_color(row.button, selected ? colors.primary : colors.border, 0);

  const auto text_color =
      selected ? (focused_ ? selected_text : unfocused_selected_text) : normal_text;
  lv_obj_set_style_text_color(row.text_label, text_color, 0);
  lv_obj_set_style_text_color(row.icon_label, text_color, 0);
}

void IconList::scroll_selected_to_view_() {
  if (selected_index_ >= rows_.size() || !rows_[selected_index_].button) {
    return;
  }

  lv_obj_scroll_to_view(rows_[selected_index_].button, LV_ANIM_OFF);
}

void IconList::item_clicked_cb(lv_event_t* event) {
  auto* list   = static_cast<IconList*>(lv_event_get_user_data(event));
  auto* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
  if (!list || !target) {
    return;
  }

  for (const auto& row : list->rows_) {
    if (row.button == target) {
      list->set_selected_index(row.index);
      if (list->clicked_callback_) {
        list->clicked_callback_(row.index, list->user_data_);
      }
      return;
    }
  }
}

}  // namespace view::widgets
