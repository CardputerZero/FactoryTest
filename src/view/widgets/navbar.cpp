/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "navbar.h"

#include <cstdint>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace view::widgets {
namespace {

struct IconSpec {
  const char* text;
  lv_event_cb_t cb;
  lv_event_code_t event_code;
  void* user_data;
};

constexpr std::array<int32_t, 5> K_NAV_BUTTON_X_OFFSETS = {36, 17, 2, -17, -36};
constexpr uint32_t K_LONG_PRESS_PROGRESS_MS             = 700;
constexpr uint32_t K_LCD_NAV_DEBOUNCE_MS                = 220;
constexpr uint32_t K_LCD_COMPLETE_LOCKOUT_MS            = 450;
constexpr uint32_t K_START_EXIT_DEBOUNCE_MS             = 650;

}  // namespace

NavBar::NavBar(lv_obj_t* parent,
               viewmodel::AppViewModel& app_view_model,
               app::AssetManager& assets,
               viewmodel::LcdTestViewModel* lcd_view_model,
               viewmodel::StartMenuViewModel* start_menu_view_model,
               viewmodel::ConnectivityTestViewModel* connectivity_view_model)
    : BaseWidgets(parent),
      app_view_model_(app_view_model),
      assets_(assets),
      lcd_view_model_(lcd_view_model),
      start_menu_view_model_(start_menu_view_model),
      connectivity_view_model_(connectivity_view_model) {}

NavBar::~NavBar() {
  lv_async_call_cancel(update_icons_async_cb, this);

  for (size_t i = 0; i < icon_buttons_.size(); ++i) {
    if (icon_buttons_[i]) {
      platform::unregister_nav_button(i, icon_buttons_[i]->root());
    }
  }

  if (page_observer_) {
    lv_observer_remove(page_observer_);
    page_observer_ = nullptr;
  }
  if (app_theme_observer_) {
    lv_observer_remove(app_theme_observer_);
    app_theme_observer_ = nullptr;
  }
  if (brightness_observer_) {
    lv_observer_remove(brightness_observer_);
    brightness_observer_ = nullptr;
  }
  if (connectivity_page_observer_) {
    lv_observer_remove(connectivity_page_observer_);
    connectivity_page_observer_ = nullptr;
  }
  if (start_exit_overlay_ && lv_obj_is_valid(start_exit_overlay_)) {
    lv_obj_delete(start_exit_overlay_);
    start_exit_overlay_       = nullptr;
    start_exit_overlay_label_ = nullptr;
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
  page_observer_ = reactive::observe_obj(core_obj_,
                                         app_view_model_.current_page_subject(),
                                         update_icons_cb,
                                         this);
  app_theme_observer_ =
      reactive::observe_obj(core_obj_, app_view_model_.dark_mode_subject(), update_icons_cb, this);
  if (lcd_view_model_) {
    brightness_observer_ = reactive::observe_obj(core_obj_,
                                                 lcd_view_model_->brightness_test_active_subject(),
                                                 update_icons_cb,
                                                 this);
  }
  if (connectivity_view_model_) {
    connectivity_page_observer_ = reactive::observe_obj(core_obj_,
                                                        connectivity_view_model_->active_page_subject(),
                                                        update_icons_cb,
                                                        this);
  }
  update_icon_buttons_();
}

void NavBar::create_icon_buttons_() {
  const auto light_color = view::palette(false).text;
  const auto dark_color  = view::palette(true).text;
  icon_font_             = assets_.load_font("Phosphor-Fill.ttf", 26);

  for (size_t i = 0; i < icon_buttons_.size(); ++i) {
    icon_buttons_[i] =
        std::make_unique<IconButton>(core_obj_,
                                     app_view_model_,
                                     32,
                                     22,
                                     "",
                                     icon_font_ ? icon_font_ : &lv_font_montserrat_14,
                                     light_color,
                                     dark_color,
                                     nullptr,
                                     &app_view_model_);
    icon_buttons_[i]->build();
    lv_obj_set_style_translate_x(icon_buttons_[i]->root(), K_NAV_BUTTON_X_OFFSETS[i], 0);
    platform::register_nav_button(i, icon_buttons_[i]->root());
  }
}

void NavBar::update_icon_buttons_() {
  std::array<IconSpec, 5> keyboard_icons = {{
      {ICON_ARROW_U_UP_LEFT, request_back_or_quit_cb, LV_EVENT_LONG_PRESSED, this},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {ICON_CHECK_SQUARE, complete_test_cb, LV_EVENT_LONG_PRESSED, &app_view_model_},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {ICON_X_SQUARE, complete_test_cb, LV_EVENT_LONG_PRESSED, &app_view_model_},
  }};
  std::array<IconSpec, 5> start_icons    = {{
      {ICON_SIGN_OUT, start_exit_confirm_cb, LV_EVENT_LONG_PRESSED, this},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {ICON_CHECK_SQUARE, confirm_start_menu_cb, LV_EVENT_CLICKED, this},
  }};
  const bool brightness_active = lcd_view_model_ && lcd_view_model_->is_brightness_test_active();
  std::array<IconSpec, 5> lcd_icons          = {{
      {ICON_ARROW_U_UP_LEFT, request_back_or_quit_cb, LV_EVENT_CLICKED, this},
      {brightness_active ? ICON_MINUS : "",
       brightness_active ? decrease_lcd_brightness_cb : nullptr,
       LV_EVENT_CLICKED,
       this},
      {ICON_CHECK_SQUARE,
       brightness_active ? complete_lcd_test_cb : advance_lcd_color_cb,
       LV_EVENT_CLICKED,
       this},
      {brightness_active ? ICON_PLUS : "",
       brightness_active ? increase_lcd_brightness_cb : nullptr,
       LV_EVENT_CLICKED,
       this},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
  }};
  std::array<IconSpec, 5> placeholder_icons  = {{
      {ICON_ARROW_U_UP_LEFT, request_back_or_quit_cb, LV_EVENT_CLICKED, this},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {ICON_CHECK_SQUARE, complete_test_cb, LV_EVENT_CLICKED, &app_view_model_},
  }};
  std::array<IconSpec, 5> connectivity_icons = {{
      {ICON_ARROW_U_UP_LEFT, request_back_or_quit_cb, LV_EVENT_CLICKED, this},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {ICON_CHECK_SQUARE, complete_test_cb, LV_EVENT_CLICKED, &app_view_model_},
  }};
  std::array<IconSpec, 5> link_test_icons = {{
      {ICON_ARROW_U_UP_LEFT, request_back_or_quit_cb, LV_EVENT_CLICKED, this},
      {ICON_ARROWS_CLOCKWISE, restart_link_test_cb, LV_EVENT_CLICKED, this},
      {"", nullptr, LV_EVENT_CLICKED, nullptr},
      {ICON_GEAR_FINE, show_link_settings_cb, LV_EVENT_CLICKED, this},
      {ICON_CHECK_SQUARE, complete_test_cb, LV_EVENT_CLICKED, &app_view_model_},
  }};

  const auto current_page  = app_view_model_.current_page();
  const bool keyboard_page = current_page == model::AppPage::KEYBOARD_TEST;
  const bool link_test_page =
      current_page == model::AppPage::CONNECTIVITY_TEST && connectivity_view_model_ &&
      connectivity_view_model_->active_page() == model::ConnectivitySubPage::LINK_TEST;
  if (current_page != model::AppPage::START) {
    reset_start_exit_hold_();
  }
  const auto& icons = current_page == model::AppPage::START               ? start_icons
                      : current_page == model::AppPage::KEYBOARD_TEST     ? keyboard_icons
                      : current_page == model::AppPage::LCD_TEST          ? lcd_icons
                      : link_test_page                                    ? link_test_icons
                      : current_page == model::AppPage::CONNECTIVITY_TEST ? connectivity_icons
                                                                          : placeholder_icons;
  for (size_t i = 0; i < icon_buttons_.size(); ++i) {
    auto& button = icon_buttons_[i];
    if (!button) {
      continue;
    }

    button->set_text(icons[i].text);
    if (!button->root()) {
      continue;
    }

    lv_obj_remove_event_cb(button->root(), request_back_or_quit_cb);
    lv_obj_remove_event_cb(button->root(), start_exit_press_cb);
    lv_obj_remove_event_cb(button->root(), start_exit_confirm_cb);
    lv_obj_remove_event_cb(button->root(), start_exit_release_cb);
    lv_obj_remove_event_cb(button->root(), complete_test_cb);
    lv_obj_remove_event_cb(button->root(), complete_lcd_test_cb);
    lv_obj_remove_event_cb(button->root(), confirm_start_menu_cb);
    lv_obj_remove_event_cb(button->root(), advance_lcd_color_cb);
    lv_obj_remove_event_cb(button->root(), decrease_lcd_brightness_cb);
    lv_obj_remove_event_cb(button->root(), increase_lcd_brightness_cb);
    lv_obj_remove_event_cb(button->root(), restart_link_test_cb);
    lv_obj_remove_event_cb(button->root(), show_link_settings_cb);
    lv_obj_remove_event_cb(button->root(), start_hold_progress_cb);
    lv_obj_remove_event_cb(button->root(), reset_hold_progress_cb);

    // Keep all five placeholders in the flex layout so 4/5/6/7/8 positions stay fixed.
    lv_obj_remove_flag(button->root(), LV_OBJ_FLAG_HIDDEN);
    button->clear_hold_target_color();
    if (keyboard_page && icons[i].cb) {
      const auto colors = view::palette(app_view_model_.is_dark_mode());
      button->set_hold_target_color(i == 2 ? colors.success : colors.error);
    } else if (current_page == model::AppPage::START && i == 0) {
      button->set_hold_target_color(view::palette(app_view_model_.is_dark_mode()).error);
    }

    if (icons[i].cb) {
      lv_obj_add_event_cb(button->root(), icons[i].cb, icons[i].event_code, icons[i].user_data);
      button->set_enabled(true);
      if (current_page == model::AppPage::START && i == 0) {
        lv_obj_add_event_cb(button->root(), start_exit_press_cb, LV_EVENT_PRESSED, this);
        lv_obj_add_event_cb(button->root(), start_exit_release_cb, LV_EVENT_RELEASED, this);
        lv_obj_add_event_cb(button->root(), start_exit_release_cb, LV_EVENT_PRESS_LOST, this);
      } else if (keyboard_page) {
        lv_obj_add_event_cb(button->root(), start_hold_progress_cb, LV_EVENT_PRESSED, button.get());
        lv_obj_add_event_cb(button->root(),
                            reset_hold_progress_cb,
                            LV_EVENT_RELEASED,
                            button.get());
        lv_obj_add_event_cb(button->root(),
                            reset_hold_progress_cb,
                            LV_EVENT_PRESS_LOST,
                            button.get());
      }
    } else {
      button->set_enabled(false);
    }
  }
}

void NavBar::schedule_icon_update_() {
  if (icon_update_scheduled_) {
    return;
  }

  if (lv_async_call(update_icons_async_cb, this) == LV_RESULT_OK) {
    icon_update_scheduled_ = true;
  } else {
    update_icon_buttons_();
  }
}

bool NavBar::can_trigger_lcd_color_() {
  if (has_lcd_color_trigger_ && lv_tick_elaps(last_lcd_color_trigger_at_) < K_LCD_NAV_DEBOUNCE_MS) {
    return false;
  }

  last_lcd_color_trigger_at_ = lv_tick_get();
  has_lcd_color_trigger_     = true;
  return true;
}

bool NavBar::can_trigger_lcd_brightness_() {
  if (has_lcd_brightness_trigger_ &&
      lv_tick_elaps(last_lcd_brightness_trigger_at_) < K_LCD_NAV_DEBOUNCE_MS) {
    return false;
  }

  last_lcd_brightness_trigger_at_ = lv_tick_get();
  has_lcd_brightness_trigger_     = true;
  return true;
}

bool NavBar::can_complete_lcd_test_() {
  return !has_lcd_color_trigger_ ||
         lv_tick_elaps(last_lcd_color_trigger_at_) >= K_LCD_COMPLETE_LOCKOUT_MS;
}

bool NavBar::can_start_exit_() const {
  return !has_back_to_start_debounce_ ||
         lv_tick_elaps(last_back_to_start_at_) >= K_START_EXIT_DEBOUNCE_MS;
}

void NavBar::mark_start_exit_debounce_() {
  last_back_to_start_at_      = lv_tick_get();
  has_back_to_start_debounce_ = true;
  start_exit_hold_active_     = false;
}

void NavBar::show_start_exit_overlay_() {
  if (!parent_) {
    return;
  }

  if (!start_exit_overlay_) {
    const auto palette  = view::palette(app_view_model_.is_dark_mode());
    start_exit_overlay_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(start_exit_overlay_);
    lv_obj_set_size(start_exit_overlay_, 236, 44);
    lv_obj_align(start_exit_overlay_, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_radius(start_exit_overlay_, 12, 0);
    lv_obj_set_style_bg_color(start_exit_overlay_, palette.button, 0);
    lv_obj_set_style_bg_opa(start_exit_overlay_, LV_OPA_90, 0);
    lv_obj_set_style_border_color(start_exit_overlay_, palette.border, 0);
    lv_obj_set_style_border_width(start_exit_overlay_, 1, 0);
    lv_obj_set_style_shadow_width(start_exit_overlay_, 12, 0);
    lv_obj_set_style_shadow_opa(start_exit_overlay_, LV_OPA_20, 0);
    lv_obj_clear_flag(start_exit_overlay_, LV_OBJ_FLAG_SCROLLABLE);

    start_exit_overlay_label_ = lv_label_create(start_exit_overlay_);
    lv_label_set_text(start_exit_overlay_label_, "Hold ESC/4 to Exit");
    lv_obj_set_style_text_color(start_exit_overlay_label_, palette.text, 0);
    lv_obj_set_style_text_font(start_exit_overlay_label_, &lv_font_montserrat_14, 0);
    lv_obj_center(start_exit_overlay_label_);
  } else {
    const auto palette = view::palette(app_view_model_.is_dark_mode());
    lv_obj_set_style_bg_color(start_exit_overlay_, palette.button, 0);
    lv_obj_set_style_border_color(start_exit_overlay_, palette.border, 0);
    if (start_exit_overlay_label_) {
      lv_obj_set_style_text_color(start_exit_overlay_label_, palette.text, 0);
    }
  }

  lv_obj_move_foreground(start_exit_overlay_);
  lv_obj_remove_flag(start_exit_overlay_, LV_OBJ_FLAG_HIDDEN);
}

void NavBar::hide_start_exit_overlay_() {
  if (start_exit_overlay_ && lv_obj_is_valid(start_exit_overlay_)) {
    lv_obj_add_flag(start_exit_overlay_, LV_OBJ_FLAG_HIDDEN);
  }
}

void NavBar::reset_start_exit_hold_() {
  start_exit_hold_active_ = false;
  hide_start_exit_overlay_();
  if (icon_buttons_[0]) {
    icon_buttons_[0]->reset_hold_progress();
  }
}

void NavBar::request_back_or_quit_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (!nav_bar) {
    return;
  }

  const bool was_start = nav_bar->app_view_model_.current_page() == model::AppPage::START;
  nav_bar->app_view_model_.request_back_or_quit();
  if (!was_start && nav_bar->app_view_model_.current_page() == model::AppPage::START) {
    nav_bar->mark_start_exit_debounce_();
  }
}

void NavBar::start_exit_press_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (!nav_bar || nav_bar->app_view_model_.current_page() != model::AppPage::START ||
      !nav_bar->can_start_exit_()) {
    return;
  }

  nav_bar->start_exit_hold_active_ = true;
  nav_bar->show_start_exit_overlay_();
  if (nav_bar->icon_buttons_[0]) {
    nav_bar->icon_buttons_[0]->start_hold_progress(K_LONG_PRESS_PROGRESS_MS);
  }
}

void NavBar::start_exit_confirm_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (!nav_bar || nav_bar->app_view_model_.current_page() != model::AppPage::START ||
      !nav_bar->start_exit_hold_active_ || !nav_bar->can_start_exit_()) {
    return;
  }

  nav_bar->reset_start_exit_hold_();
  nav_bar->app_view_model_.request_quit();
}

void NavBar::start_exit_release_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (nav_bar) {
    nav_bar->reset_start_exit_hold_();
  }
}

void NavBar::complete_test_cb(lv_event_t* event) {
  auto* view_model = static_cast<viewmodel::AppViewModel*>(lv_event_get_user_data(event));
  if (view_model) {
    view_model->complete_current_test();
  }
}

void NavBar::complete_lcd_test_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (nav_bar && nav_bar->can_complete_lcd_test_()) {
    nav_bar->app_view_model_.complete_current_test();
  }
}

void NavBar::confirm_start_menu_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (!nav_bar || !nav_bar->start_menu_view_model_) {
    return;
  }

  const auto& item = nav_bar->start_menu_view_model_->selected_item();
  if (item.starts_sequence) {
    nav_bar->app_view_model_.start_full_test_sequence();
    return;
  }

  nav_bar->app_view_model_.show_single_test_page(item.target_page);
}

void NavBar::advance_lcd_color_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (nav_bar && nav_bar->lcd_view_model_ && nav_bar->can_trigger_lcd_color_()) {
    nav_bar->lcd_view_model_->advance_color();
  }
}

void NavBar::decrease_lcd_brightness_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (nav_bar && nav_bar->lcd_view_model_ && nav_bar->can_trigger_lcd_brightness_()) {
    nav_bar->lcd_view_model_->decrease_brightness();
  }
}

void NavBar::increase_lcd_brightness_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (nav_bar && nav_bar->lcd_view_model_ && nav_bar->can_trigger_lcd_brightness_()) {
    nav_bar->lcd_view_model_->increase_brightness();
  }
}

void NavBar::restart_link_test_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (nav_bar && nav_bar->connectivity_view_model_) {
    nav_bar->connectivity_view_model_->request_link_restart();
  }
}

void NavBar::show_link_settings_cb(lv_event_t* event) {
  auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
  if (nav_bar && nav_bar->connectivity_view_model_) {
    nav_bar->connectivity_view_model_->request_link_settings();
  }
}

void NavBar::start_hold_progress_cb(lv_event_t* event) {
  auto* button = static_cast<IconButton*>(lv_event_get_user_data(event));
  if (button) {
    button->start_hold_progress(K_LONG_PRESS_PROGRESS_MS);
  }
}

void NavBar::reset_hold_progress_cb(lv_event_t* event) {
  auto* button = static_cast<IconButton*>(lv_event_get_user_data(event));
  if (button) {
    button->reset_hold_progress();
  }
}

void NavBar::update_icons_cb(lv_observer_t* observer, lv_subject_t* subject) {
  LV_UNUSED(subject);

  auto* nav_bar = static_cast<NavBar*>(lv_observer_get_user_data(observer));
  if (nav_bar) {
    nav_bar->schedule_icon_update_();
  }
}

void NavBar::update_icons_async_cb(void* user_data) {
  auto* nav_bar = static_cast<NavBar*>(user_data);
  if (nav_bar) {
    nav_bar->icon_update_scheduled_ = false;
    nav_bar->update_icon_buttons_();
  }
}

}  // namespace view::widgets
