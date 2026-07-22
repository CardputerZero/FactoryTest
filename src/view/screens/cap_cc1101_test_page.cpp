/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cap_cc1101_test_page.h"

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_CONTENT_WIDTH = 300;
constexpr int32_t K_ROW_HEIGHT    = 25;

lv_color_t state_color(model::CapCc1101ItemState state, bool dark_mode) {
  const auto colors = view::palette(dark_mode);
  switch (state) {
    case model::CapCc1101ItemState::RUNNING:
      return colors.info;
    case model::CapCc1101ItemState::PASSED:
      return colors.success;
    case model::CapCc1101ItemState::FAILED:
      return colors.error;
    case model::CapCc1101ItemState::PENDING:
    default:
      return colors.text_muted;
  }
}

bool can_start(model::CapCc1101RunState state) {
  return state != model::CapCc1101RunState::RUNNING;
}

}  // namespace

CapCc1101TestPage::CapCc1101TestPage(viewmodel::AppViewModel& app_view_model,
                                     app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  app_view_model_ref_().clear_nav_actions();
  set_nav_action_('4', view::ICON_ARROW_U_UP_LEFT, [this]() { cancel_and_leave_(); });
  set_nav_action_('8', view::ICON_CHECK_SQUARE, [this]() { show_test_result_dialog_(); });
  init();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, 100, this);
  platform::set_key_listener(key_listener, this);
}

CapCc1101TestPage::~CapCc1101TestPage() {
  platform::clear_key_listener(key_listener, this);
  if (refresh_timer_) {
    lv_timer_delete(refresh_timer_);
    refresh_timer_ = nullptr;
  }
  model_.cancel();
}

void CapCc1101TestPage::build_content(lv_obj_t* content) {
  auto* title_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-semibold.ttf"), 11);
  auto* text_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 10);
  auto* layout = lv_obj_create(content);
  lv_obj_remove_style_all(layout);
  lv_obj_set_size(layout, K_CONTENT_WIDTH, 78);
  lv_obj_align(layout, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_flex_flow(layout, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(layout, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(layout, 2, 0);
  lv_obj_clear_flag(layout, LV_OBJ_FLAG_SCROLLABLE);

  status_label_               = lv_label_create(layout);
  const auto initial_headline = app_view_model_ref_().tr("Install CAP-CC1101, then press Enter");
  lv_label_set_text(status_label_, initial_headline.c_str());
  lv_obj_set_width(status_label_, K_CONTENT_WIDTH);
  lv_label_set_long_mode(status_label_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(status_label_, title_font ? title_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(status_label_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  const auto initial = model_.snapshot();
  for (std::size_t i = 0; i < initial.items.size(); ++i) {
    auto* row = lv_obj_create(layout);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, K_CONTENT_WIDTH, K_ROW_HEIGHT);
    lv_obj_set_style_radius(row, 5, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_pad_left(row, 8, 0);
    lv_obj_set_style_pad_right(row, 8, 0);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    reactive::bind_theme(row,
                         app_view_model_ref_().dark_mode_subject(),
                         reactive::ThemeRole::BUTTON);

    auto* name                 = lv_label_create(row);
    const auto translated_name = app_view_model_ref_().tr(initial.items[i].name);
    lv_label_set_text(name, translated_name.c_str());
    lv_obj_set_width(name, 48);
    lv_obj_set_style_text_font(name, text_font ? text_font : &lv_font_montserrat_12, 0);
    reactive::bind_theme(name,
                         app_view_model_ref_().dark_mode_subject(),
                         reactive::ThemeRole::TEXT);

    status_values_[i]  = lv_label_create(row);
    const auto waiting = app_view_model_ref_().tr("WAIT");
    lv_label_set_text(status_values_[i], waiting.c_str());
    lv_obj_set_width(status_values_[i], 34);
    lv_obj_set_style_text_font(status_values_[i],
                               text_font ? text_font : &lv_font_montserrat_12,
                               0);

    detail_values_[i] = lv_label_create(row);
    const auto translated_detail = app_view_model_ref_().tr(initial.items[i].detail.c_str());
    lv_label_set_text(detail_values_[i], translated_detail.c_str());
    lv_obj_set_width(detail_values_[i], 190);
    lv_label_set_long_mode(detail_values_[i], LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(detail_values_[i],
                               text_font ? text_font : &lv_font_montserrat_12,
                               0);
    reactive::bind_theme(detail_values_[i],
                         app_view_model_ref_().dark_mode_subject(),
                         reactive::ThemeRole::TEXT);
  }
}

void CapCc1101TestPage::refresh_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<CapCc1101TestPage*>(lv_timer_get_user_data(timer));
  if (page) {
    page->refresh_();
  }
}

void CapCc1101TestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<CapCc1101TestPage*>(user_data);
  if (!page) {
    return;
  }
  if (page->handle_test_result_dialog_key_(key, key_name)) {
    return;
  }
  if (key == LV_KEY_ENTER || key == '\n' || key == '\r') {
    page->start_();
  }
}

void CapCc1101TestPage::refresh_() {
  const auto snapshot = model_.snapshot();
  if (snapshot.revision == displayed_revision_) {
    return;
  }
  displayed_revision_            = snapshot.revision;
  const auto translated_headline = app_view_model_ref_().tr(snapshot.headline.c_str());
  lv_label_set_text(status_label_, translated_headline.c_str());
  for (std::size_t i = 0; i < snapshot.items.size(); ++i) {
    const auto translated_state =
        app_view_model_ref_().tr(cap_cc1101_item_state_text(snapshot.items[i].state));
    const auto translated_detail = app_view_model_ref_().tr(snapshot.items[i].detail.c_str());
    lv_label_set_text(status_values_[i], translated_state.c_str());
    lv_label_set_text(detail_values_[i], translated_detail.c_str());
    lv_obj_set_style_text_color(
        status_values_[i],
        state_color(snapshot.items[i].state, app_view_model_ref_().is_dark_mode()),
        0);
  }
}

void CapCc1101TestPage::start_() {
  if (can_start(model_.snapshot().state)) {
    model_.start();
  }
}

void CapCc1101TestPage::cancel_and_leave_() {
  model_.cancel();
  app_view_model_ref_().show_start_page();
}

}  // namespace screen
