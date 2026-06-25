/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

#include <array>
#include <memory>

#include "app_viewmodel.h"
#include "base_widget.h"
#include "icon_button.h"

namespace app {
class AssetManager;
}

namespace view::widgets {

class NavBar : public BaseWidgets {
 public:
  NavBar(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~NavBar() override;

  void build() override;

 protected:
  void create_icon_buttons_();
  void update_icon_buttons_();
  void set_button_hold_style_(std::size_t index, const viewmodel::NavAction& action);
  std::size_t index_for_button_(lv_obj_t* button) const;
  void trigger_button_(lv_obj_t* button, lv_event_code_t event_code);
  static void nav_button_cb(lv_event_t* event);
  static void nav_button_press_cb(lv_event_t* event);
  static void nav_button_release_cb(lv_event_t* event);
  static void update_icons_cb(lv_observer_t* observer, lv_subject_t* subject);

 private:
  viewmodel::AppViewModel& app_view_model_;
  app::AssetManager& assets_;
  std::array<std::unique_ptr<IconButton>, 5> icon_buttons_{};
  lv_font_t* icon_font_{nullptr};
  lv_observer_t* nav_actions_observer_{nullptr};
  lv_observer_t* app_theme_observer_{nullptr};
};

}  // namespace view::widgets
