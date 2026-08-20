/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "precheck_power_dialog.h"

#include <algorithm>

#include "theme.h"
#include "ui_const.h"

namespace precheck {
namespace {

struct PowerOption {
  const char* icon;
  const char* label;
  PowerAction action;
};

constexpr PowerOption K_OPTIONS[] = {
    {view::ICON_POWER, "关机", PowerAction::POWER_OFF},
    {view::ICON_ARROWS_CLOCKWISE, "重启", PowerAction::REBOOT},
    {view::ICON_X_SQUARE, "取消", PowerAction::CANCEL},
};

constexpr int32_t K_ROW_HEIGHT = 54;

}  // namespace

PowerDialog::PowerDialog(lv_obj_t* parent,
                         const lv_font_t* text_font,
                         const lv_font_t* icon_font,
                         ActionCallback callback,
                         void* user_data)
    : parent_(parent),
      text_font_(text_font),
      icon_font_(icon_font),
      callback_(callback),
      user_data_(user_data) {}

PowerDialog::~PowerDialog() {
  if (backdrop_) {
    lv_obj_delete(backdrop_);
  }
}

void PowerDialog::open() {
  if (!backdrop_) {
    build_();
  }
  selected_index_ = 2;
  visible_ = true;
  lv_obj_remove_flag(backdrop_, LV_OBJ_FLAG_HIDDEN);
  update_selection_();
}

void PowerDialog::close() {
  if (!backdrop_) {
    return;
  }
  visible_ = false;
  lv_obj_add_flag(backdrop_, LV_OBJ_FLAG_HIDDEN);
}

bool PowerDialog::handle_key(uint32_t key) {
  if (!visible_) {
    return false;
  }
  if (key == LV_KEY_ESC) {
    activate_(PowerAction::CANCEL);
    return true;
  }
  if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
    selected_index_ = selected_index_ == 0 ? rows_.size() - 1 : selected_index_ - 1;
    update_selection_();
    return true;
  }
  if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
    selected_index_ = (selected_index_ + 1) % rows_.size();
    update_selection_();
    return true;
  }
  if (key == LV_KEY_ENTER) {
    activate_selected_();
    return true;
  }
  return true;
}

void PowerDialog::build_() {
  backdrop_ = lv_obj_create(parent_ ? parent_ : lv_layer_top());
  lv_obj_remove_style_all(backdrop_);
  lv_obj_set_size(backdrop_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(backdrop_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(backdrop_, LV_OPA_60, 0);
  lv_obj_add_event_cb(backdrop_, backdrop_clicked_cb, LV_EVENT_CLICKED, this);

  const auto colors = view::palette(true);
  const auto* display = lv_display_get_default();
  const int32_t screen_width = display ? lv_display_get_horizontal_resolution(display) : 1024;
  const int32_t screen_height = display ? lv_display_get_vertical_resolution(display) : 600;
  const int32_t panel_width = std::clamp(screen_width - 96, 360, 560);
  const int32_t panel_height = 286;

  panel_ = lv_obj_create(backdrop_);
  lv_obj_remove_style_all(panel_);
  lv_obj_set_size(panel_, panel_width, panel_height);
  lv_obj_center(panel_);
  lv_obj_set_style_bg_color(panel_, colors.surface, 0);
  lv_obj_set_style_bg_opa(panel_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(panel_, 1, 0);
  lv_obj_set_style_border_color(panel_, colors.border, 0);
  lv_obj_set_style_radius(panel_, 10, 0);
  lv_obj_set_style_pad_all(panel_, 22, 0);
  lv_obj_set_style_pad_row(panel_, 12, 0);
  lv_obj_set_flex_flow(panel_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  auto* title = lv_label_create(panel_);
  lv_label_set_text(title, "电源选项");
  if (text_font_) lv_obj_set_style_text_font(title, text_font_, 0);
  lv_obj_set_style_text_color(title, colors.text, 0);
  lv_obj_set_width(title, LV_PCT(100));
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  auto* hint = lv_label_create(panel_);
  lv_label_set_text(hint, "上下选择，Enter 确认，Esc 返回");
  if (text_font_) lv_obj_set_style_text_font(hint, text_font_, 0);
  lv_obj_set_style_text_color(hint, colors.text_muted, 0);
  lv_obj_set_width(hint, LV_PCT(100));
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

  list_ = lv_obj_create(panel_);
  lv_obj_remove_style_all(list_);
  lv_obj_set_width(list_, LV_PCT(100));
  lv_obj_set_flex_grow(list_, 1);
  lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(list_, 6, 0);
  lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);

  rows_.clear();
  for (const auto& option : K_OPTIONS) {
    Row row{};
    row.action = option.action;
    row.button = lv_button_create(list_);
    lv_obj_remove_style_all(row.button);
    lv_obj_set_width(row.button, LV_PCT(100));
    lv_obj_set_height(row.button, K_ROW_HEIGHT);
    lv_obj_set_style_radius(row.button, 6, 0);
    lv_obj_set_style_pad_hor(row.button, 16, 0);
    lv_obj_clear_flag(row.button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row.button, row_clicked_cb, LV_EVENT_CLICKED, this);

    row.icon = lv_label_create(row.button);
    lv_label_set_text(row.icon, option.icon);
    if (icon_font_) lv_obj_set_style_text_font(row.icon, icon_font_, 0);
    lv_obj_set_width(row.icon, 36);
    lv_obj_set_style_text_align(row.icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(row.icon, LV_ALIGN_LEFT_MID, 0, 0);

    row.label = lv_label_create(row.button);
    lv_label_set_text(row.label, option.label);
    if (text_font_) lv_obj_set_style_text_font(row.label, text_font_, 0);
    lv_obj_align(row.label, LV_ALIGN_LEFT_MID, 52, 0);
    rows_.push_back(row);
  }

  // Keep the modal centered even on displays smaller than the preferred height.
  if (screen_height < panel_height + 20) {
    lv_obj_set_height(panel_, std::max(screen_height - 20, 180));
  }
  lv_obj_add_flag(backdrop_, LV_OBJ_FLAG_HIDDEN);
  update_selection_();
}

void PowerDialog::activate_selected_() {
  if (selected_index_ < rows_.size()) {
    activate_(rows_[selected_index_].action);
  }
}

void PowerDialog::activate_(PowerAction action) {
  close();
  // Cancel only dismisses this dialog; it must never be interpreted as a
  // request to cancel the active precheck run.
  if (callback_ && action != PowerAction::CANCEL) {
    callback_(action, user_data_);
  }
}

void PowerDialog::update_selection_() {
  if (rows_.empty()) return;
  const auto colors = view::palette(true);
  for (std::size_t i = 0; i < rows_.size(); ++i) {
    const bool selected = i == selected_index_;
    const auto bg = selected ? lv_color_mix(colors.primary, colors.surface, 38) : colors.surface;
    lv_obj_set_style_bg_opa(rows_[i].button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(rows_[i].button, bg, 0);
    lv_obj_set_style_border_width(rows_[i].button, selected ? 1 : 0, 0);
    lv_obj_set_style_border_color(rows_[i].button, colors.primary, 0);
    lv_obj_set_style_text_color(rows_[i].icon, selected ? colors.primary : colors.text_muted, 0);
    lv_obj_set_style_text_color(rows_[i].label, selected ? colors.text : colors.text_muted, 0);
  }
  if (selected_index_ < rows_.size()) {
    lv_obj_scroll_to_view(rows_[selected_index_].button, LV_ANIM_OFF);
  }
}

void PowerDialog::row_clicked_cb(lv_event_t* event) {
  auto* dialog = static_cast<PowerDialog*>(lv_event_get_user_data(event));
  auto* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
  if (!dialog || !target) return;
  for (std::size_t i = 0; i < dialog->rows_.size(); ++i) {
    if (dialog->rows_[i].button == target) {
      dialog->selected_index_ = i;
      dialog->update_selection_();
      dialog->activate_selected_();
      return;
    }
  }
}

void PowerDialog::backdrop_clicked_cb(lv_event_t* event) {
  auto* dialog = static_cast<PowerDialog*>(lv_event_get_user_data(event));
  if (dialog && lv_event_get_target(event) == dialog->backdrop_) {
    dialog->activate_(PowerAction::CANCEL);
  }
}

}  // namespace precheck
