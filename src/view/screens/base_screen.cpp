/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_screen.h"

#include <array>
#include <cstdio>
#include <utility>

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {

BaseScreen::BaseScreen(viewmodel::AppViewModel& app_view_model,
                       app::AssetManager& assets)
    : app_view_model_(app_view_model),
      assets_(assets) {}

BaseScreen::~BaseScreen() {
  test_result_dialog_.reset();
  title_bar_.reset();
  nav_bar_.reset();

  if (root_ && lv_obj_is_valid(root_)) {
    lv_obj_delete(root_);
  }
}

void BaseScreen::init() {
  if (root_) {
    return;
  }

  root_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, view::K_SCREEN_WIDTH, view::K_SCREEN_HEIGHT);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(root_, app_view_model_.dark_mode_subject(), reactive::ThemeRole::SCREEN);

  title_bar_ = std::make_unique<view::widgets::TitleBar>(root_, app_view_model_);
  title_bar_->build();

  nav_bar_ = std::make_unique<view::widgets::NavBar>(root_, app_view_model_, assets_);
  nav_bar_->build();

  content_ = lv_obj_create(root_);
  lv_obj_remove_style_all(content_);
  lv_obj_set_size(content_,
                  LV_PCT(100),
                  view::K_SCREEN_HEIGHT - view::K_TITLE_BAR_HEIGHT - view::K_NAV_BAR_HEIGHT);
  lv_obj_align(content_, LV_ALIGN_TOP_MID, 0, view::K_TITLE_BAR_HEIGHT);
  lv_obj_clear_flag(content_, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(content_, app_view_model_.dark_mode_subject(), reactive::ThemeRole::SURFACE);

  build_content(content_);
}

lv_obj_t* BaseScreen::root() const { return root_; }

viewmodel::AppViewModel& BaseScreen::app_view_model_ref_() { return app_view_model_; }

app::AssetManager& BaseScreen::assets_ref_() { return assets_; }

void BaseScreen::set_nav_action_(uint32_t keypad,
                                 const char* icon,
                                 std::function<void()> action,
                                 lv_event_code_t event_code,
                                 viewmodel::NavHoldTarget hold_target,
                                 bool force_enabled,
                                 std::function<void()> press_action,
                                 std::function<void()> release_action) {
  viewmodel::NavAction nav_action;
  nav_action.icon          = icon ? icon : "";
  nav_action.action        = std::move(action);
  nav_action.press_action  = std::move(press_action);
  nav_action.release_action = std::move(release_action);
  nav_action.event_code    = event_code;
  nav_action.hold_target   = hold_target;
  nav_action.force_enabled = force_enabled;
  app_view_model_.set_keypad_nav_action(keypad, std::move(nav_action));
}

void BaseScreen::set_default_test_nav_(bool show_complete) {
  std::array<viewmodel::NavAction, 5> actions{};
  viewmodel::NavAction back;
  back.icon   = view::ICON_ARROW_U_UP_LEFT;
  back.action = [this]() { app_view_model_.request_back_or_quit(); };
  actions[0]  = std::move(back);

  if (show_complete) {
    viewmodel::NavAction complete;
    complete.icon   = view::ICON_CHECK_SQUARE;
    complete.action = [this]() { show_test_result_dialog_(); };
    actions[4]      = std::move(complete);
  }

  app_view_model_.set_nav_actions(std::move(actions));
}

void BaseScreen::show_test_result_dialog_() {
  if (!root_) {
    return;
  }
  if (test_result_dialog_ && test_result_dialog_->visible()) {
    return;
  }

  view::widgets::DialogConfig config;
  config.width               = 286;
  config.height              = 146;
  config.title               = app_view_model_.current_test_name();
  config.shortcut_text       = "Z/C: Select  Enter: Apply";
  config.ok_button_label     = "Pass";
  config.skip_button_label   = "Skip";
  config.cancel_button_label = "Fail";
  config.body_font_size      = 14;
  config.body_offset_y       = -10;
  config.shortcut_width      = 190;
  config.button_width        = 62;
  config.button_row_width    = 232;
  config.button_bottom_pad   = 2;
  config.ok_button_tone      = view::widgets::DialogButtonTone::SUCCESS;
  config.skip_button_tone    = view::widgets::DialogButtonTone::WARNING;
  config.cancel_button_tone  = view::widgets::DialogButtonTone::ERROR;
  config.show_skip_button    = true;
  config.use_nav_action_keys = false;
  config.focus_button_navigation = true;

  view::widgets::DialogCallbacks callbacks;
  callbacks.ok_action = [this]() {
    app_view_model_.complete_current_test(model::TestResult::PASS);
  };
  callbacks.skip_action = [this]() {
    app_view_model_.complete_current_test(model::TestResult::SKIPPED);
  };
  callbacks.cancel_action = [this]() {
    app_view_model_.complete_current_test(model::TestResult::FAILED);
  };

  test_result_dialog_ =
      std::make_unique<view::widgets::Dialog>(root_, app_view_model_, assets_, config, callbacks);
  test_result_dialog_->build();
  char body[48]{};
  const auto current = app_view_model_.current_test_number();
  const auto total   = app_view_model_.test_count();
  if (current > 0 && total > 0) {
    std::snprintf(body, sizeof(body), "%zu/%zu - Test Result", current, total);
  } else {
    std::snprintf(body, sizeof(body), "Test Result");
  }
  test_result_dialog_->add_label(body, 246, LV_TEXT_ALIGN_CENTER);
}

bool BaseScreen::handle_test_result_dialog_key_(uint32_t key, const char* key_name) {
  return test_result_dialog_ && test_result_dialog_->handle_key(key, key_name);
}

view::widgets::TitleBar* BaseScreen::title_bar_ref_() { return title_bar_.get(); }

}  // namespace screen
