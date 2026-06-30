/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "lcd_test_page.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <utility>

#include "asset_manager.h"
#include "backlight_service.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

struct LcdColorStep {
  lv_color_t color;
  lv_color_t text_color;
  const char* label;
};

const std::array<LcdColorStep, model::LcdTestModel::K_COLOR_STEP_COUNT>& lcd_color_steps() {
  static const std::array<LcdColorStep, model::LcdTestModel::K_COLOR_STEP_COUNT> STEPS = {{
      {lv_color_hex(0xff0000), lv_color_hex(0xffffff), "RED"},
      {lv_color_hex(0x00ff00), lv_color_hex(0x000000), "GREEN"},
      {lv_color_hex(0x0000ff), lv_color_hex(0xffffff), "BLUE"},
      {lv_color_hex(0xffffff), lv_color_hex(0x000000), "WHITE"},
      {lv_color_hex(0x000000), lv_color_hex(0xffffff), "BLACK"},
      {lv_color_hex(0x808080), lv_color_hex(0xffffff), "GRAY 50%"},
      {lv_color_hex(0xc0c0c0), lv_color_hex(0x000000), "GRAY 75%"},
      {lv_color_hex(0xffff00), lv_color_hex(0x000000), "YELLOW"},
      {lv_color_hex(0x00ffff), lv_color_hex(0x000000), "CYAN"},
  }};
  return STEPS;
}

constexpr uint32_t K_BRIGHTNESS_COMMIT_DELAY_MS = 320;
constexpr uint32_t K_BRIGHTNESS_SMOOTH_STEP_MS  = 20;
constexpr int32_t K_BRIGHTNESS_SMOOTH_STEP      = 2;

bool is_tab_input(uint32_t key, const char* key_name) {
  if (key == '\t') {
    return true;
  }
#ifdef LV_KEY_NEXT
  if (key == LV_KEY_NEXT) {
    return true;
  }
#endif
  if (!key_name) {
    return false;
  }
  return std::strcmp(key_name, "Tab") == 0 || std::strcmp(key_name, "Next") == 0;
}

}  // namespace

LcdTestPage::LcdTestPage(viewmodel::AppViewModel& app_view_model,
                         viewmodel::LcdTestViewModel& lcd_view_model,
                         app::AssetManager& assets)
    : BaseScreen(app_view_model, assets),
      lcd_view_model_(lcd_view_model) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  lcd_view_model_.reset_test();
  update_nav_actions_();
  init();
  platform::set_key_listener(key_listener, this);
}

LcdTestPage::~LcdTestPage() {
  platform::clear_key_listener(key_listener, this);
  if (color_observer_handle_) {
    lv_observer_remove(color_observer_handle_);
    color_observer_handle_ = nullptr;
  }
  if (brightness_active_observer_handle_) {
    lv_observer_remove(brightness_active_observer_handle_);
    brightness_active_observer_handle_ = nullptr;
  }
  if (brightness_percent_observer_handle_) {
    lv_observer_remove(brightness_percent_observer_handle_);
    brightness_percent_observer_handle_ = nullptr;
  }
  if (theme_observer_handle_) {
    lv_observer_remove(theme_observer_handle_);
    theme_observer_handle_ = nullptr;
  }
  if (brightness_commit_timer_) {
    lv_timer_delete(brightness_commit_timer_);
    brightness_commit_timer_ = nullptr;
  }
  if (brightness_smooth_timer_) {
    lv_timer_delete(brightness_smooth_timer_);
    brightness_smooth_timer_ = nullptr;
  }
}

void LcdTestPage::build_content(lv_obj_t* content) {
  tileview_ = lv_tileview_create(content);
  lv_obj_set_size(tileview_, LV_PCT(100), LV_PCT(100));
  lv_obj_center(tileview_);
  lv_obj_set_scrollbar_mode(tileview_, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(tileview_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tileview_, 0, 0);
  lv_obj_set_style_pad_all(tileview_, 0, 0);
  lv_obj_add_event_cb(tileview_, tile_scroll_end_cb, LV_EVENT_SCROLL_END, this);

  tiles_[K_COLOR_TILE_INDEX] =
      lv_tileview_add_tile(tileview_, K_COLOR_TILE_INDEX, 0, LV_DIR_RIGHT);
  tiles_[K_BRIGHTNESS_TILE_INDEX] =
      lv_tileview_add_tile(tileview_, K_BRIGHTNESS_TILE_INDEX, 0, LV_DIR_LEFT);
  for (auto* tile : tiles_) {
    lv_obj_set_style_bg_opa(tile, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
  }

  prompt_ = lv_label_create(tiles_[K_COLOR_TILE_INDEX]);
  lv_label_set_text(prompt_, "Press Enter to step through LCD colors.");
  lv_obj_set_width(prompt_, 280);
  lv_obj_set_style_text_align(prompt_, LV_TEXT_ALIGN_CENTER, 0);
  auto* prompt_font = assets_ref_().load_font("inter-medium.ttf", 16);
  lv_obj_set_style_text_font(prompt_, prompt_font ? prompt_font : &lv_font_montserrat_16, 0);
  lv_obj_center(prompt_);
  reactive::bind_theme(prompt_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  brightness_group_ = lv_obj_create(tiles_[K_BRIGHTNESS_TILE_INDEX]);
  lv_obj_remove_style_all(brightness_group_);
  lv_obj_set_size(brightness_group_, 300, 118);
  lv_obj_set_flex_flow(brightness_group_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(brightness_group_,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(brightness_group_, 12, 0);
  lv_obj_clear_flag(brightness_group_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(brightness_group_);

  brightness_label_ = lv_label_create(brightness_group_);
  lv_obj_set_width(brightness_label_, 290);
  auto* label_font = assets_ref_().load_font("inter-medium.ttf", 16);
  lv_obj_set_style_text_font(brightness_label_,
                             label_font ? label_font : &lv_font_montserrat_16,
                             0);
  lv_obj_set_style_text_align(brightness_label_, LV_TEXT_ALIGN_CENTER, 0);
  reactive::bind_theme(brightness_label_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  brightness_slider_ = lv_slider_create(brightness_group_);
  lv_obj_set_width(brightness_slider_, 260);
  lv_slider_set_range(brightness_slider_, model::LcdTestModel::K_MIN_BRIGHTNESS_PERCENT, 100);
  lv_obj_clear_flag(brightness_slider_, LV_OBJ_FLAG_CLICKABLE);
  apply_brightness_theme_(app_view_model_ref_().is_dark_mode());

  color_layer_ = lv_obj_create(root());
  lv_obj_remove_style_all(color_layer_);
  lv_obj_set_size(color_layer_, LV_PCT(100), LV_PCT(100));
  lv_obj_align(color_layer_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(color_layer_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(color_layer_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);

  auto* color_label = lv_label_create(color_layer_);
  lv_label_set_text(color_label, "");
  auto* color_label_font = assets_ref_().load_font("inter-semibold.ttf", 24);
  lv_obj_set_style_text_font(color_label,
                             color_label_font ? color_label_font : &lv_font_montserrat_24,
                             0);
  lv_obj_center(color_label);

  color_observer_handle_ = reactive::observe_obj(color_layer_,
                                                 lcd_view_model_.color_index_subject(),
                                                 color_observer,
                                                 this);
  brightness_active_observer_handle_ =
      reactive::observe_obj(brightness_group_,
                            lcd_view_model_.brightness_test_active_subject(),
                            brightness_active_observer,
                            this);
  brightness_percent_observer_handle_ =
      reactive::observe_obj(brightness_group_,
                            lcd_view_model_.brightness_percent_subject(),
                            brightness_percent_observer,
                            this);
  theme_observer_handle_ = reactive::observe_obj(brightness_slider_,
                                                 app_view_model_ref_().dark_mode_subject(),
                                                 theme_observer,
                                                 this);
  apply_color_index_(0);
  apply_brightness_active_(lcd_view_model_.is_brightness_test_active());
  apply_brightness_percent_(model::LcdTestModel::K_INITIAL_BRIGHTNESS_PERCENT);
  show_tile_(K_COLOR_TILE_INDEX, false);
}

void LcdTestPage::apply_color_index_(int32_t index) {
  if (!prompt_ || !color_layer_ || !brightness_group_) {
    return;
  }
  current_color_index_ = index;

  if (lcd_view_model_.is_brightness_test_active()) {
    lv_obj_add_flag(prompt_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
    show_tile_(K_BRIGHTNESS_TILE_INDEX, true);
    return;
  }

  if (index <= 0) {
    lv_label_set_text(prompt_, "Press Enter to step through LCD colors.");
    lv_obj_remove_flag(prompt_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  if (index > model::LcdTestModel::K_COLOR_STEP_COUNT) {
    lv_label_set_text(prompt_, "LCD color test complete.");
    lv_obj_remove_flag(prompt_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  const auto& steps                  = lcd_color_steps();
  const auto& step                   = steps[static_cast<std::size_t>(index - 1)];
  const bool was_color_layer_visible = !lv_obj_has_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(prompt_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_opa(color_layer_, LV_OPA_COVER, 0);
  if (was_color_layer_visible) {
    view::animate_style_color(color_layer_, LV_STYLE_BG_COLOR, step.color, 180);
  } else {
    lv_obj_set_style_bg_color(color_layer_, step.color, 0);
  }

  if (auto* label = lv_obj_get_child(color_layer_, 0)) {
    lv_label_set_text(label, step.label);
    if (was_color_layer_visible) {
      view::animate_style_color(label, LV_STYLE_TEXT_COLOR, step.text_color, 180);
    } else {
      lv_obj_set_style_text_color(label, step.text_color, 0);
    }
    lv_obj_center(label);
  }
}

void LcdTestPage::apply_brightness_active_(bool active) {
  if (!prompt_ || !color_layer_ || !brightness_group_) {
    return;
  }

  if (active) {
    lv_obj_add_flag(prompt_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
    show_tile_(K_BRIGHTNESS_TILE_INDEX, true);
    update_nav_actions_();
    return;
  }

  update_nav_actions_();
}

void LcdTestPage::apply_brightness_percent_(int32_t percent) {
  requested_brightness_percent_ =
      std::clamp(percent, model::LcdTestModel::K_MIN_BRIGHTNESS_PERCENT, 100);
  if (brightness_slider_) {
    lv_slider_set_value(brightness_slider_, requested_brightness_percent_, LV_ANIM_ON);
  }
  if (brightness_label_) {
    static char buffer[48];
    lv_snprintf(buffer,
                sizeof(buffer),
                "Display Brightness: %d%%",
                static_cast<int>(requested_brightness_percent_));
    lv_label_set_text(brightness_label_, buffer);
  }

  if (lcd_view_model_.is_brightness_test_active()) {
    schedule_brightness_commit_();
  }
}

void LcdTestPage::apply_brightness_theme_(bool dark_mode) {
  view::apply_slider_theme(brightness_slider_, dark_mode);
}

void LcdTestPage::update_nav_actions_() {
  std::array<viewmodel::NavAction, 5> actions{};

  viewmodel::NavAction back;
  back.icon   = view::ICON_ARROW_U_UP_LEFT;
  back.action = [this]() { app_view_model_ref_().request_back_or_quit(); };
  actions[0]  = std::move(back);

  if (active_tile_index_ == K_BRIGHTNESS_TILE_INDEX) {
    viewmodel::NavAction decrease;
    decrease.icon   = view::ICON_MINUS;
    decrease.action = [this]() { lcd_view_model_.decrease_brightness(); };
    actions[1]      = std::move(decrease);

    viewmodel::NavAction increase;
    increase.icon   = view::ICON_PLUS;
    increase.action = [this]() { lcd_view_model_.increase_brightness(); };
    actions[3]      = std::move(increase);
  }

  viewmodel::NavAction complete;
  complete.icon   = view::ICON_CHECK_SQUARE;
  complete.action = [this]() { show_test_result_dialog_(); };
  actions[4]      = std::move(complete);

  app_view_model_ref_().set_nav_actions(std::move(actions));
}

void LcdTestPage::advance_color_step_() {
  if (active_tile_index_ != K_COLOR_TILE_INDEX || lcd_view_model_.is_brightness_test_active()) {
    return;
  }
  lcd_view_model_.advance_color();
}

void LcdTestPage::show_tile_(std::size_t index, bool animate) {
  if (!tileview_ || index >= tiles_.size() || !tiles_[index]) {
    return;
  }

  if (index == K_BRIGHTNESS_TILE_INDEX && !lcd_view_model_.is_brightness_test_active()) {
    lcd_view_model_.start_brightness_test();
    return;
  }

  active_tile_index_ = index;
  update_nav_actions_();
  if (color_layer_) {
    if (active_tile_index_ == K_COLOR_TILE_INDEX && current_color_index_ > 0 &&
        current_color_index_ <= model::LcdTestModel::K_COLOR_STEP_COUNT &&
        !lcd_view_model_.is_brightness_test_active()) {
      lv_obj_remove_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
    }
  }
  lv_tileview_set_tile(tileview_, tiles_[index], animate ? LV_ANIM_ON : LV_ANIM_OFF);
}

void LcdTestPage::switch_to_next_tile_() {
  if (lcd_view_model_.is_brightness_test_active()) {
    show_tile_(K_BRIGHTNESS_TILE_INDEX, true);
    return;
  }
  const auto next = active_tile_index_ == K_COLOR_TILE_INDEX ? K_BRIGHTNESS_TILE_INDEX
                                                             : K_COLOR_TILE_INDEX;
  show_tile_(next, true);
}

void LcdTestPage::sync_active_tile_() {
  if (!tileview_) {
    return;
  }

  auto* active_tile = lv_tileview_get_tile_active(tileview_);
  for (std::size_t i = 0; i < tiles_.size(); ++i) {
    if (active_tile == tiles_[i]) {
      active_tile_index_ = i;
      update_nav_actions_();
      if (color_layer_) {
        if (active_tile_index_ == K_COLOR_TILE_INDEX && current_color_index_ > 0 &&
            current_color_index_ <= model::LcdTestModel::K_COLOR_STEP_COUNT &&
            !lcd_view_model_.is_brightness_test_active()) {
          lv_obj_remove_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_add_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
        }
      }
      return;
    }
  }
}

void LcdTestPage::schedule_brightness_commit_() {
  if (brightness_smooth_timer_) {
    lv_timer_pause(brightness_smooth_timer_);
  }

  if (!brightness_commit_timer_) {
    brightness_commit_timer_ =
        lv_timer_create(brightness_commit_timer_cb, K_BRIGHTNESS_COMMIT_DELAY_MS, this);
    lv_timer_set_auto_delete(brightness_commit_timer_, false);
    lv_timer_set_repeat_count(brightness_commit_timer_, 1);
    return;
  }

  lv_timer_set_repeat_count(brightness_commit_timer_, 1);
  lv_timer_resume(brightness_commit_timer_);
  lv_timer_reset(brightness_commit_timer_);
}

void LcdTestPage::begin_brightness_transition_() {
  target_brightness_percent_ =
      std::clamp(requested_brightness_percent_, model::LcdTestModel::K_MIN_BRIGHTNESS_PERCENT, 100);

  if (!hardware_brightness_loaded_) {
    int32_t current = 0;
    if (platform::backlight::read_brightness_percent(current)) {
      hardware_brightness_percent_ = current;
    } else {
      hardware_brightness_percent_ = target_brightness_percent_;
    }
    hardware_brightness_loaded_ = true;
  }

  if (hardware_brightness_percent_ == target_brightness_percent_) {
    platform::backlight::write_brightness_percent(target_brightness_percent_);
    return;
  }

  if (!brightness_smooth_timer_) {
    brightness_smooth_timer_ =
        lv_timer_create(brightness_smooth_timer_cb, K_BRIGHTNESS_SMOOTH_STEP_MS, this);
    return;
  }

  lv_timer_resume(brightness_smooth_timer_);
  lv_timer_reset(brightness_smooth_timer_);
}

void LcdTestPage::step_brightness_transition_() {
  const int32_t delta = target_brightness_percent_ - hardware_brightness_percent_;
  if (delta == 0) {
    if (brightness_smooth_timer_) {
      lv_timer_pause(brightness_smooth_timer_);
    }
    return;
  }

  const int32_t step =
      std::min<int32_t>(std::abs(delta), K_BRIGHTNESS_SMOOTH_STEP) * (delta > 0 ? 1 : -1);
  hardware_brightness_percent_ = std::clamp(hardware_brightness_percent_ + step,
                                            model::LcdTestModel::K_MIN_BRIGHTNESS_PERCENT,
                                            100);
  platform::backlight::write_brightness_percent(hardware_brightness_percent_);

  if (hardware_brightness_percent_ == target_brightness_percent_ && brightness_smooth_timer_) {
    lv_timer_pause(brightness_smooth_timer_);
  }
}

void LcdTestPage::color_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<LcdTestPage*>(lv_observer_get_user_data(observer));
  if (!page) {
    return;
  }

  page->apply_color_index_(lv_subject_get_int(subject));
}

void LcdTestPage::brightness_active_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<LcdTestPage*>(lv_observer_get_user_data(observer));
  if (!page) {
    return;
  }

  page->apply_brightness_active_(lv_subject_get_int(subject) != 0);
}

void LcdTestPage::brightness_percent_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<LcdTestPage*>(lv_observer_get_user_data(observer));
  if (!page) {
    return;
  }

  page->apply_brightness_percent_(lv_subject_get_int(subject));
}

void LcdTestPage::theme_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<LcdTestPage*>(lv_observer_get_user_data(observer));
  if (page) {
    page->apply_brightness_theme_(lv_subject_get_int(subject) != 0);
  }
}

void LcdTestPage::tile_scroll_end_cb(lv_event_t* event) {
  auto* page = static_cast<LcdTestPage*>(lv_event_get_user_data(event));
  if (page) {
    page->sync_active_tile_();
  }
}

void LcdTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<LcdTestPage*>(user_data);
  if (!page) {
    return;
  }
  if (page->handle_test_result_dialog_key_(key, key_name)) {
    return;
  }

  if (is_tab_input(key, key_name)) {
    page->switch_to_next_tile_();
    return;
  }

  switch (key) {
    case LV_KEY_ENTER:
      page->advance_color_step_();
      break;
    case LV_KEY_UP:
    case LV_KEY_RIGHT:
    case '7':
    case 'x':
    case 'X':
      page->lcd_view_model_.increase_brightness();
      break;
    case LV_KEY_DOWN:
    case LV_KEY_LEFT:
    case '5':
    case 'f':
    case 'F':
      page->lcd_view_model_.decrease_brightness();
      break;
    default:
      break;
  }
}

void LcdTestPage::brightness_commit_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<LcdTestPage*>(lv_timer_get_user_data(timer));
  if (!page) {
    return;
  }

  page->begin_brightness_transition_();
}

void LcdTestPage::brightness_smooth_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<LcdTestPage*>(lv_timer_get_user_data(timer));
  if (!page) {
    return;
  }

  page->step_brightness_transition_();
}

}  // namespace screen
