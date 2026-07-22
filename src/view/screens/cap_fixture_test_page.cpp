/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cap_fixture_test_page.h"

#include <algorithm>
#include <array>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_VIEWPORT_WIDTH  = 300;
constexpr int32_t K_VIEWPORT_HEIGHT = 106;
constexpr int32_t K_LIST_WIDTH      = 292;
constexpr int32_t K_ROW_HEIGHT      = 32;
constexpr int32_t K_SCROLL_STEP     = 38;

lv_color_t state_color(model::CapFixtureItemState state, bool dark_mode) {
  const auto colors = view::palette(dark_mode);
  switch (state) {
    case model::CapFixtureItemState::RUNNING:
      return colors.info;
    case model::CapFixtureItemState::PASSED:
      return colors.success;
    case model::CapFixtureItemState::FAILED:
      return colors.error;
    case model::CapFixtureItemState::PENDING:
    default:
      return colors.text_muted;
  }
}

}  // namespace

CapFixtureTestPage::CapFixtureTestPage(viewmodel::AppViewModel& app_view_model,
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

CapFixtureTestPage::~CapFixtureTestPage() {
  platform::clear_key_listener(key_listener, this);
  if (refresh_timer_) {
    lv_timer_delete(refresh_timer_);
    refresh_timer_ = nullptr;
  }
  model_.cancel();
}

void CapFixtureTestPage::build_content(lv_obj_t* content) {
  auto* headline_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-semibold.ttf"), 12);
  auto* text_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 10);

  viewport_ = lv_obj_create(content);
  lv_obj_remove_style_all(viewport_);
  lv_obj_set_size(viewport_, K_VIEWPORT_WIDTH, K_VIEWPORT_HEIGHT);
  lv_obj_align(viewport_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(viewport_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(viewport_, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_set_scroll_dir(viewport_, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(viewport_, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_pad_all(viewport_, 0, 0);
  reactive::bind_theme(viewport_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);

  grid_ = lv_obj_create(viewport_);
  lv_obj_remove_style_all(grid_);
  lv_obj_set_width(grid_, K_LIST_WIDTH);
  lv_obj_set_height(grid_, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grid_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(grid_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(grid_, 6, 0);
  lv_obj_set_style_pad_top(grid_, 3, 0);
  lv_obj_set_style_pad_bottom(grid_, 6, 0);
  lv_obj_clear_flag(grid_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(grid_, LV_ALIGN_TOP_MID, 0, 0);

  headline_label_             = lv_label_create(grid_);
  const auto initial_headline = app_view_model_ref_().tr("Install CAP fixture, then press Enter");
  lv_label_set_text(headline_label_, initial_headline.c_str());
  lv_obj_set_width(headline_label_, K_LIST_WIDTH);
  lv_obj_set_style_text_align(headline_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(headline_label_,
                             headline_font ? headline_font : &lv_font_montserrat_12,
                             0);
  reactive::bind_theme(headline_label_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  const auto initial = model_.snapshot();
  for (std::size_t i = 0; i < initial.items.size(); ++i) {
    auto* row       = lv_obj_create(grid_);
    row_objects_[i] = row;
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, K_LIST_WIDTH, K_ROW_HEIGHT);
    lv_obj_set_style_radius(row, 5, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_pad_left(row, 10, 0);
    lv_obj_set_style_pad_right(row, 10, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    reactive::bind_theme(row,
                         app_view_model_ref_().dark_mode_subject(),
                         reactive::ThemeRole::BUTTON);

    auto* name                 = lv_label_create(row);
    const auto translated_name = app_view_model_ref_().tr(initial.items[i].name);
    lv_label_set_text(name, translated_name.c_str());
    lv_obj_set_width(name, 52);
    lv_obj_set_style_text_font(name, text_font ? text_font : &lv_font_montserrat_12, 0);
    reactive::bind_theme(name,
                         app_view_model_ref_().dark_mode_subject(),
                         reactive::ThemeRole::TEXT);

    status_labels_[i]        = lv_label_create(row);
    const auto waiting_state = app_view_model_ref_().tr("WAIT");
    lv_label_set_text(status_labels_[i], waiting_state.c_str());
    lv_obj_set_width(status_labels_[i], 40);
    lv_obj_set_style_text_font(status_labels_[i],
                               text_font ? text_font : &lv_font_montserrat_12,
                               0);

    detail_labels_[i]         = lv_label_create(row);
    const auto waiting_detail = app_view_model_ref_().tr("Waiting");
    lv_label_set_text(detail_labels_[i], waiting_detail.c_str());
    lv_obj_set_width(detail_labels_[i], 164);
    lv_label_set_long_mode(detail_labels_[i], LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(detail_labels_[i],
                               text_font ? text_font : &lv_font_montserrat_12,
                               0);
    reactive::bind_theme(detail_labels_[i],
                         app_view_model_ref_().dark_mode_subject(),
                         reactive::ThemeRole::TEXT);
  }

  error_label_ = lv_label_create(grid_);
  const auto protocol_hint =
      app_view_model_ref_().tr("Fixed-delay sequence; see terminal for protocol logs");
  lv_label_set_text(error_label_, protocol_hint.c_str());
  lv_obj_set_width(error_label_, K_LIST_WIDTH - 8);
  lv_label_set_long_mode(error_label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(error_label_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_text_font(error_label_, text_font ? text_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(error_label_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);
}

void CapFixtureTestPage::refresh_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<CapFixtureTestPage*>(lv_timer_get_user_data(timer));
  if (page) {
    page->refresh_();
  }
}

void CapFixtureTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<CapFixtureTestPage*>(user_data);
  if (!page) {
    return;
  }
  if (page->handle_test_result_dialog_key_(key, key_name)) {
    return;
  }
  if (key == LV_KEY_ENTER || key == '\n' || key == '\r') {
    page->start_();
    return;
  }
  switch (key) {
    case LV_KEY_UP:
    case 'f':
    case 'F':
      page->scroll_(-1);
      break;
    case LV_KEY_DOWN:
    case 'x':
    case 'X':
      page->scroll_(1);
      break;
    default:
      break;
  }
}

void CapFixtureTestPage::start_() {
  if (model_.snapshot().state == model::CapFixtureRunState::IDLE) {
    model_.start();
  }
}

void CapFixtureTestPage::refresh_() {
  const auto snapshot = model_.snapshot();
  const bool terminal = snapshot.state == model::CapFixtureRunState::PASSED ||
                        snapshot.state == model::CapFixtureRunState::FAILED;
  if (snapshot.revision != displayed_revision_) {
    displayed_revision_            = snapshot.revision;
    const auto translated_headline = app_view_model_ref_().tr(snapshot.headline.c_str());
    lv_label_set_text(headline_label_, translated_headline.c_str());
    for (std::size_t i = 0; i < snapshot.items.size(); ++i) {
      const auto translated_state =
          app_view_model_ref_().tr(model::cap_fixture_item_state_text(snapshot.items[i].state));
      const auto translated_detail = app_view_model_ref_().tr(snapshot.items[i].detail.c_str());
      lv_label_set_text(status_labels_[i], translated_state.c_str());
      lv_label_set_text(detail_labels_[i], translated_detail.c_str());
      lv_obj_set_style_text_color(
          status_labels_[i],
          state_color(snapshot.items[i].state, app_view_model_ref_().is_dark_mode()),
          0);
    }
    const auto error_text = app_view_model_ref_().tr(
        snapshot.error_message.empty() ? "Fixed-delay sequence; see terminal for protocol logs"
                                       : snapshot.error_message.c_str());
    lv_label_set_text(error_label_, error_text.c_str());
    if (terminal) {
      lv_obj_scroll_to_view_recursive(error_label_, LV_ANIM_ON);
    } else if (snapshot.state == model::CapFixtureRunState::RUNNING &&
               snapshot.active_index < row_objects_.size()) {
      lv_obj_scroll_to_view_recursive(row_objects_[snapshot.active_index], LV_ANIM_ON);
    }
  }
}

void CapFixtureTestPage::scroll_(int32_t direction) {
  if (!viewport_ || direction == 0) {
    return;
  }
  lv_obj_update_layout(viewport_);
  const int32_t current_y = lv_obj_get_scroll_y(viewport_);
  const int32_t max_y     = current_y + lv_obj_get_scroll_bottom(viewport_);
  const int32_t target_y  = std::clamp(current_y + direction * K_SCROLL_STEP, 0, max_y);
  lv_obj_scroll_to_y(viewport_, target_y, LV_ANIM_ON);
}

void CapFixtureTestPage::cancel_and_leave_() {
  model_.cancel();
  app_view_model_ref_().show_start_page();
}

}  // namespace screen
