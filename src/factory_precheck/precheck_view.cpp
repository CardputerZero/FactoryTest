/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "precheck_view.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "theme.h"

namespace precheck {
namespace {

lv_color_t color_for(CheckState state, const view::ThemePalette& colors) {
  switch (state) {
    case CheckState::RUNNING: return colors.info;
    case CheckState::PASSED: return colors.success;
    case CheckState::FAILED: return colors.error;
    case CheckState::CANCELED: return colors.warning;
    case CheckState::SKIPPED: return colors.text_muted;
    case CheckState::PENDING: default: return colors.text_muted;
  }
}

}  // namespace

PrecheckView::PrecheckView(PrecheckViewModel& view_model, app::AssetManager& assets)
    : view_model_(view_model), assets_(assets) {}

PrecheckView::~PrecheckView() {
  if (refresh_timer_) {
    lv_timer_delete(refresh_timer_);
  }
}

void PrecheckView::build(lv_display_t* display) {
  const auto colors = view::palette(true);
  root_ = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_color(root_, colors.background, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(root_, 24, 0);
  lv_obj_set_style_pad_row(root_, 10, 0);

  const auto width = lv_display_get_horizontal_resolution(display);
  const auto title_size = width < 900 ? 24U : 30U;
  const auto text_size = width < 900 ? 18U : 21U;
  const auto small_size = width < 900 ? 16U : 18U;
  title_font_ = assets_.load_font_exact("alibaba-puhui-regular.ttf", title_size);
  text_font_ = assets_.load_font_exact("alibaba-puhui-regular.ttf", text_size);
  small_font_ = assets_.load_font_exact("alibaba-puhui-regular.ttf", small_size);
  icon_font_ = assets_.load_font_exact("Phosphor-Fill.ttf", text_size);

  title_ = lv_label_create(root_);
  lv_obj_set_width(title_, LV_PCT(92));
  lv_obj_set_style_text_font(title_, title_font_ ? title_font_ : &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title_, colors.text, 0);
  lv_label_set_text(title_, "设备自动初检");

  device_ = lv_label_create(root_);
  lv_obj_set_width(device_, LV_PCT(92));
  lv_obj_set_style_text_font(device_, text_font_ ? text_font_ : &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(device_, colors.text_muted, 0);
  progress_ = lv_bar_create(root_);
  lv_obj_set_width(progress_, LV_PCT(92));
  lv_obj_set_height(progress_, 18);
  lv_obj_set_style_bg_color(progress_, colors.button, 0);
  lv_obj_set_style_bg_color(progress_, colors.primary, LV_PART_INDICATOR);
  status_ = lv_label_create(root_);
  lv_obj_set_width(status_, LV_PCT(92));
  lv_obj_set_style_text_font(status_, small_font_ ? small_font_ : &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(status_, colors.text, 0);
  auto* body = lv_obj_create(root_);
  lv_obj_remove_style_all(body);
  lv_obj_set_width(body, LV_PCT(92));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(body, 20, 0);

  list_ = lv_obj_create(body);
  lv_obj_remove_style_all(list_);
  lv_obj_set_height(list_, LV_PCT(100));
  lv_obj_set_flex_grow(list_, 7);
  lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(list_, 5, 0);
  lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list_, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_AUTO);

  const auto initial = view_model_.snapshot();
  item_rows_.reserve(initial.items.size() * 3);

  auto build_sub_rows = [&](const auto& sub_items, std::vector<lv_obj_t*>& sub_rows) {
    sub_rows.reserve(sub_items.size() * 3);
    for (const auto& sub : sub_items) {
      auto* sub_row = lv_obj_create(list_);
      lv_obj_remove_style_all(sub_row);
      lv_obj_set_width(sub_row, LV_PCT(100));
      lv_obj_set_height(sub_row, LV_SIZE_CONTENT);
      lv_obj_set_flex_flow(sub_row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(sub_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(sub_row, 12, 0);
      lv_obj_set_style_pad_left(sub_row, 22, 0);
      auto* name = lv_label_create(sub_row);
      lv_obj_set_width(name, 64);
      lv_obj_set_style_text_font(name, small_font_ ? small_font_ : &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(name, colors.text_muted, 0);
      lv_label_set_text(name, sub.name.c_str());
      auto* sub_state = lv_label_create(sub_row);
      lv_obj_set_width(sub_state, 70);
      lv_obj_set_style_text_font(sub_state,
                                 small_font_ ? small_font_ : &lv_font_montserrat_14,
                                 0);
      lv_label_set_text(sub_state, check_state_text(sub.state));
      auto* sub_detail = lv_label_create(sub_row);
      lv_obj_set_flex_grow(sub_detail, 1);
      lv_label_set_long_mode(sub_detail, LV_LABEL_LONG_DOT);
      lv_obj_set_style_text_font(sub_detail,
                                 small_font_ ? small_font_ : &lv_font_montserrat_14,
                                 0);
      lv_label_set_text(sub_detail, sub.detail.c_str());
      sub_rows.push_back(name);
      sub_rows.push_back(sub_state);
      sub_rows.push_back(sub_detail);
    }
  };

  for (const auto& item : initial.items) {
    auto* row = lv_obj_create(list_);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 12, 0);
    auto* state = lv_label_create(row);
    lv_obj_set_width(state, 70);
    lv_obj_set_style_text_font(state, small_font_ ? small_font_ : &lv_font_montserrat_14, 0);
    lv_label_set_text(state, "等待");
    auto* text = lv_label_create(row);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_style_text_font(text, small_font_ ? small_font_ : &lv_font_montserrat_14, 0);
    lv_label_set_text(text, item.name.c_str());
    auto* detail = lv_label_create(row);
    lv_obj_set_width(detail, 220);
    lv_label_set_long_mode(detail, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(detail, small_font_ ? small_font_ : &lv_font_montserrat_14, 0);
    lv_label_set_text(detail, item.detail.c_str());
    item_rows_.push_back(state);
    item_rows_.push_back(text);
    item_rows_.push_back(detail);

    // CAP 治具聚合检测展开到左侧待检区：在父行下方逐项列出治具子项。
    if (item.id == "cap_fixture") {
      build_sub_rows(item.cap_sub_items, cap_sub_rows_);
    } else if (item.id == "ir_fixture") {
      build_sub_rows(item.ir_sub_items, ir_sub_rows_);
    }
  }

  detail_panel_ = lv_obj_create(body);
  lv_obj_remove_style_all(detail_panel_);
  lv_obj_set_height(detail_panel_, LV_PCT(100));
  lv_obj_set_flex_grow(detail_panel_, 3);
  lv_obj_set_flex_flow(detail_panel_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(detail_panel_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(detail_panel_, 12, 0);
  lv_obj_set_style_pad_row(detail_panel_, 10, 0);

  // 右侧为静态说明：这是一个内部检测程序，按照下方按键提示操作。
  detail_title_ = lv_label_create(detail_panel_);
  lv_obj_set_width(detail_title_, LV_PCT(100));
  lv_obj_set_style_text_align(detail_title_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(detail_title_, text_font_ ? text_font_ : &lv_font_montserrat_18, 0);
  lv_label_set_text(detail_title_, "使用说明");

  detail_text_ = lv_label_create(detail_panel_);
  lv_obj_set_width(detail_text_, LV_PCT(100));
  lv_label_set_long_mode(detail_text_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(detail_text_, small_font_ ? small_font_ : &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(detail_text_, colors.text_muted, 0);
  lv_label_set_text(detail_text_,
                    "本程序为工厂内部检测程序，"
                    "请按照下方按键提示进行操作。\n"
                    "Enter 开始 / 停止自动初检；\n"
                    "R 重试失败项目，Esc 取消；\n"
                    "P 打开电源菜单（关机 / 重启）；\n"
                    "Q 退出程序。\n"
                    "检测过程中请勿断电，异常请查看日志。");

  summary_ = lv_label_create(root_);
  lv_obj_set_width(summary_, LV_PCT(92));
  lv_obj_set_style_text_font(summary_, small_font_ ? small_font_ : &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(summary_, colors.text_muted, 0);
  lv_obj_align(summary_, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  power_dialog_ = std::make_unique<PowerDialog>(
      lv_layer_top(), text_font_, icon_font_, power_action_cb, this);

  refresh_timer_ = lv_timer_create(refresh_timer_cb, 100, this);
  refresh_();
  LV_UNUSED(display);
}

void PrecheckView::refresh_() {
  if (!root_) return;
  const auto snapshot = view_model_.snapshot();
  const auto colors = view::palette(true);
  lv_label_set_text_fmt(device_, "%s    SN: %s", snapshot.device_name.c_str(), snapshot.serial_number.c_str());
  const int percent = snapshot.items.empty() ? 0 : static_cast<int>(snapshot.completed * 100 / snapshot.items.size());
  lv_bar_set_value(progress_, percent, LV_ANIM_OFF);
  lv_label_set_text_fmt(status_, "%s    %s", snapshot.headline.c_str(), run_state_text(snapshot.state));
  for (std::size_t i = 0; i < snapshot.items.size() && i * 3 + 2 < item_rows_.size(); ++i) {
    const auto& item = snapshot.items[i];
    lv_obj_set_style_text_color(item_rows_[i * 3], color_for(item.state, colors), 0);
    lv_label_set_text(item_rows_[i * 3], check_state_text(item.state));
    lv_label_set_text(item_rows_[i * 3 + 1], item.name.c_str());
    lv_label_set_text(item_rows_[i * 3 + 2], item.detail.c_str());
  }

  // 当前检测项变化时，自动滚动到对应行（列表较长，避免检测项在视野之外）。
  if (snapshot.current_index != displayed_current_index_ &&
      snapshot.current_index * 3 + 2 < item_rows_.size()) {
    displayed_current_index_ = snapshot.current_index;
    lv_obj_scroll_to_view_recursive(item_rows_[snapshot.current_index * 3], LV_ANIM_ON);
  }

  for (const auto& item : snapshot.items) {
    if (item.id == "cap_fixture") {
      const auto count = std::min(item.cap_sub_items.size(), cap_sub_rows_.size() / 3);
      for (std::size_t i = 0; i < count; ++i) {
        const auto& sub = item.cap_sub_items[i];
        lv_obj_set_style_text_color(cap_sub_rows_[i * 3 + 1], color_for(sub.state, colors), 0);
        lv_label_set_text(cap_sub_rows_[i * 3], sub.name.c_str());
        lv_label_set_text(cap_sub_rows_[i * 3 + 1], check_state_text(sub.state));
        lv_label_set_text(cap_sub_rows_[i * 3 + 2], sub.detail.c_str());
      }
    } else if (item.id == "ir_fixture") {
      const auto count = std::min(item.ir_sub_items.size(), ir_sub_rows_.size() / 3);
      for (std::size_t i = 0; i < count; ++i) {
        const auto& sub = item.ir_sub_items[i];
        lv_obj_set_style_text_color(ir_sub_rows_[i * 3 + 1], color_for(sub.state, colors), 0);
        lv_label_set_text(ir_sub_rows_[i * 3], sub.name.c_str());
        lv_label_set_text(ir_sub_rows_[i * 3 + 1], check_state_text(sub.state));
        lv_label_set_text(ir_sub_rows_[i * 3 + 2], sub.detail.c_str());
      }
    }
  }

  lv_label_set_text_fmt(summary_, "进度 %zu/%zu    通过 %zu    失败 %zu    Enter 开始  R 重试  Esc 取消  P 电源 Q 退出",
                        snapshot.completed, snapshot.items.size(), snapshot.passed, snapshot.failed);
}

void PrecheckView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<PrecheckView*>(lv_timer_get_user_data(timer));
  if (view) view->refresh_();
}

void PrecheckView::key_listener(uint32_t key, bool pressed, void* user_data) {
  auto* view = static_cast<PrecheckView*>(user_data);
  if (!view) return;
  if (!pressed) return;
  if (view->power_dialog_ && view->power_dialog_->visible()) {
    view->power_dialog_->handle_key(key);
    return;
  }
  if (key == 'p' || key == 'P') {
    view->open_power_dialog_();
  } else if (key == LV_KEY_ENTER) {
    const auto state = view->view_model_.snapshot().state;
    state == RunState::RUNNING ? view->view_model_.cancel() : view->view_model_.start();
  } else if (key == 'r') {
    view->view_model_.retry_failed();
  } else if (key == LV_KEY_ESC) {
    view->view_model_.cancel();
  } else if (key == 'q') {
    view->running_ = false;
  }
}

void PrecheckView::power_action_cb(PowerAction action, void* user_data) {
  auto* view = static_cast<PrecheckView*>(user_data);
  if (view) view->handle_power_action_(action);
}

void PrecheckView::open_power_dialog_() {
  if (power_dialog_) power_dialog_->open();
}

void PrecheckView::handle_power_action_(PowerAction action) {
  if (action == PowerAction::CANCEL) return;
  std::string error_message;
  bool requested = false;
  if (action == PowerAction::POWER_OFF) {
    requested = view_model_.safe_poweroff(error_message);
  } else if (action == PowerAction::REBOOT) {
    requested = view_model_.safe_reboot(error_message);
  }
  if (requested) {
    // Stop an active check only after systemd accepted the power request.
    view_model_.cancel();
  }
}

}  // namespace precheck
