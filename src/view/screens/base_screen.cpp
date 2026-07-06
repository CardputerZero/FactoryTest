/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_screen.h"

#include <array>
#include <cstring>
#include <utility>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

bool is_dialog_cancel_key(uint32_t key, const char* key_name) {
  return key == LV_KEY_ESC || key == 27 || key == '4' ||
         (key_name &&
          (std::strcmp(key_name, "Esc") == 0 || std::strcmp(key_name, "Escape") == 0 ||
           std::strcmp(key_name, "KEY_1") == 0));
}

}  // namespace

BaseScreen::BaseScreen(viewmodel::AppViewModel& app_view_model,
                       app::AssetManager& assets)
    : app_view_model_(app_view_model),
      assets_(assets) {}

BaseScreen::~BaseScreen() {
  test_result_dialog_.reset();
  platform::set_modal_key_capture(false);
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

  title_bar_ = std::make_unique<view::widgets::TitleBar>(root_, app_view_model_, assets_);
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

  view::widgets::TestConfirmDialogConfig config;
  config.title   = app_view_model_.current_test_name();
  config.current = app_view_model_.current_test_number();
  config.total   = app_view_model_.test_count();

  view::widgets::TestConfirmDialogCallbacks callbacks;
  callbacks.pass_action = [this]() {
    platform::set_modal_key_capture(false);
    app_view_model_.complete_current_test(model::TestResult::PASS);
  };
  callbacks.skip_action = [this]() {
    platform::set_modal_key_capture(false);
    app_view_model_.complete_current_test(model::TestResult::SKIPPED);
  };
  callbacks.fail_action = [this]() {
    platform::set_modal_key_capture(false);
    app_view_model_.complete_current_test(model::TestResult::FAILED);
  };

  test_result_dialog_ = std::make_unique<view::widgets::TestConfirmDialog>(root_,
                                                                           app_view_model_,
                                                                           assets_,
                                                                           std::move(config),
                                                                           std::move(callbacks));
  platform::set_modal_key_capture(true);
  test_result_dialog_->build();
}

bool BaseScreen::handle_test_result_dialog_key_(uint32_t key, const char* key_name) {
  if (!test_result_dialog_) {
    return false;
  }
  const bool cancel_key = is_dialog_cancel_key(key, key_name);
  const bool handled    = test_result_dialog_->handle_key(key, key_name);
  if (handled && cancel_key && test_result_dialog_ && !test_result_dialog_->visible()) {
    test_result_dialog_.reset();
    platform::set_modal_key_capture(false);
  }
  return handled;
}

view::widgets::TitleBar* BaseScreen::title_bar_ref_() { return title_bar_.get(); }

}  // namespace screen
