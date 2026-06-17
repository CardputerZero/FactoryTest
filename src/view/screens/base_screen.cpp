/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

namespace screen {

BaseScreen::BaseScreen(viewmodel::AppViewModel& app_view_model,
                       app::AssetManager& assets,
                       viewmodel::LcdTestViewModel* lcd_view_model,
                       viewmodel::StartMenuViewModel* start_menu_view_model)
    : app_view_model_(app_view_model),
      assets_(assets),
      lcd_view_model_(lcd_view_model),
      start_menu_view_model_(start_menu_view_model) {}

BaseScreen::~BaseScreen() {
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

  nav_bar_ = std::make_unique<view::widgets::NavBar>(root_,
                                                     app_view_model_,
                                                     assets_,
                                                     lcd_view_model_,
                                                     start_menu_view_model_);
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

view::widgets::TitleBar* BaseScreen::title_bar_ref_() { return title_bar_.get(); }

}  // namespace screen
