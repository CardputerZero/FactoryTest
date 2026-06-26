/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "navbar.h"

#include <cstddef>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"

namespace view::widgets {
namespace {

constexpr std::array<int32_t, 5> K_NAV_BUTTON_X_OFFSETS = {36, 17, 2, -17, -36};
constexpr uint32_t K_LONG_PRESS_PROGRESS_MS             = 700;
constexpr std::size_t K_NO_NAV_INDEX                    = 5;

}  // namespace

NavBar::NavBar(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseWidgets(parent),
      app_view_model_(app_view_model),
      assets_(assets) {}

NavBar::~NavBar() {
  for (size_t i = 0; i < icon_buttons_.size(); ++i) {
    if (icon_buttons_[i]) {
      platform::unregister_nav_button(i, icon_buttons_[i]->root());
    }
  }

  if (nav_actions_observer_) {
    lv_observer_remove(nav_actions_observer_);
    nav_actions_observer_ = nullptr;
  }
  if (app_theme_observer_) {
    lv_observer_remove(app_theme_observer_);
    app_theme_observer_ = nullptr;
  }
}

void NavBar::build() {
  if (core_obj_) {
    return;
  }

  core_obj_ = lv_obj_create(parent_);
  lv_obj_remove_style_all(core_obj_);
  lv_obj_set_size(core_obj_, LV_PCT(100), K_NAV_BAR_HEIGHT);
  lv_obj_align(core_obj_, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(core_obj_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(core_obj_,
                        LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_left(core_obj_, 8, 0);
  lv_obj_set_style_pad_right(core_obj_, 8, 0);
  reactive::bind_theme(core_obj_, app_view_model_.dark_mode_subject(), reactive::ThemeRole::BAR);

  create_icon_buttons_();
  nav_actions_observer_ = reactive::observe_obj(core_obj_,
                                                app_view_model_.nav_actions_subject(),
                                                update_icons_cb,
                                                this);
  app_theme_observer_ =
      reactive::observe_obj(core_obj_, app_view_model_.dark_mode_subject(), update_icons_cb, this);
  update_icon_buttons_();
}

void NavBar::create_icon_buttons_() {
  const auto light_color = view::palette(false).text;
  const auto dark_color  = view::palette(true).text;
  icon_font_             = assets_.load_font("Phosphor-Fill.ttf", 22);

  for (size_t i = 0; i < icon_buttons_.size(); ++i) {
    icon_buttons_[i] =
        std::make_unique<IconButton>(core_obj_,
                                     app_view_model_,
                                     32,
                                     24,
                                     "",
                                     icon_font_ ? icon_font_ : &lv_font_montserrat_14,
                                     light_color,
                                     dark_color,
                                     nullptr,
                                     &app_view_model_,
                                     true);
    icon_buttons_[i]->build();
    lv_obj_set_style_translate_x(icon_buttons_[i]->root(), K_NAV_BUTTON_X_OFFSETS[i], 0);
    lv_obj_add_event_cb(icon_buttons_[i]->root(), nav_button_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(icon_buttons_[i]->root(), nav_button_cb, LV_EVENT_LONG_PRESSED, this);
    lv_obj_add_event_cb(icon_buttons_[i]->root(), nav_button_press_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(icon_buttons_[i]->root(), nav_button_release_cb, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(icon_buttons_[i]->root(), nav_button_release_cb, LV_EVENT_PRESS_LOST, this);
    platform::register_nav_button(i, icon_buttons_[i]->root());
  }
}

void NavBar::update_icon_buttons_() {
  const auto& actions = app_view_model_.nav_actions();
  for (size_t i = 0; i < icon_buttons_.size(); ++i) {
    auto& button = icon_buttons_[i];
    if (!button || !button->root()) {
      continue;
    }

    const auto& action = actions[i];
    button->set_text(action.icon.c_str());
    button->clear_hold_target_color();
    set_button_hold_style_(i, action);

    const bool has_icon = !action.icon.empty();
    button->set_enabled(has_icon && (action.action || action.force_enabled));
    lv_obj_remove_flag(button->root(), LV_OBJ_FLAG_HIDDEN);
  }
}

void NavBar::set_button_hold_style_(std::size_t index, const viewmodel::NavAction& action) {
  if (index >= icon_buttons_.size() || !icon_buttons_[index]) {
    return;
  }

  const auto colors = view::palette(app_view_model_.is_dark_mode());
  switch (action.hold_target) {
    case viewmodel::NavHoldTarget::SUCCESS:
      icon_buttons_[index]->set_hold_target_color(colors.success);
      break;
    case viewmodel::NavHoldTarget::ERROR:
      icon_buttons_[index]->set_hold_target_color(colors.error);
      break;
    case viewmodel::NavHoldTarget::NONE:
    default:
      break;
  }
}

std::size_t NavBar::index_for_button_(lv_obj_t* button) const {
  for (std::size_t i = 0; i < icon_buttons_.size(); ++i) {
    if (icon_buttons_[i] && icon_buttons_[i]->root() == button) {
      return i;
    }
  }
  return K_NO_NAV_INDEX;
}

void NavBar::trigger_button_(lv_obj_t* button, lv_event_code_t event_code) {
  const auto index = index_for_button_(button);
  if (index >= icon_buttons_.size()) {
    return;
  }
  app_view_model_.trigger_nav_action(index, event_code);
}

void NavBar::nav_button_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (!nav_bar) {
    return;
  }
  nav_bar->trigger_button_(lv_event_get_target_obj(event), lv_event_get_code(event));
}

void NavBar::nav_button_press_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (!nav_bar) {
    return;
  }
  const auto index    = nav_bar->index_for_button_(lv_event_get_target_obj(event));
  const auto& actions = nav_bar->app_view_model_.nav_actions();
  if (index < nav_bar->icon_buttons_.size() &&
      (actions[index].event_code == LV_EVENT_LONG_PRESSED ||
       actions[index].hold_target != viewmodel::NavHoldTarget::NONE)) {
    nav_bar->icon_buttons_[index]->start_hold_progress(K_LONG_PRESS_PROGRESS_MS);
  }
  if (index < nav_bar->icon_buttons_.size()) {
    nav_bar->app_view_model_.notify_nav_action_pressed(index);
  }
}

void NavBar::nav_button_release_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (!nav_bar) {
    return;
  }
  const auto index = nav_bar->index_for_button_(lv_event_get_target_obj(event));
  if (index < nav_bar->icon_buttons_.size()) {
    nav_bar->icon_buttons_[index]->reset_hold_progress();
    nav_bar->app_view_model_.notify_nav_action_released(index);
  }
}

void NavBar::update_icons_cb(lv_observer_t* observer, lv_subject_t* subject) {
  LV_UNUSED(subject);

  auto* nav_bar = static_cast<NavBar*>(lv_observer_get_user_data(observer));
  if (nav_bar) {
    nav_bar->update_icon_buttons_();
  }
}

}  // namespace view::widgets
