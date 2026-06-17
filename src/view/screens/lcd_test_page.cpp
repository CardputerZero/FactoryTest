/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "lcd_test_page.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>

#include "asset_manager.h"
#include "backlight_service.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"

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

}  // namespace

LcdTestPage::LcdTestPage(viewmodel::AppViewModel& app_view_model,
                         viewmodel::LcdTestViewModel& lcd_view_model,
                         app::AssetManager& assets)
    : BaseScreen(app_view_model, assets, &lcd_view_model),
      lcd_view_model_(lcd_view_model) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  lcd_view_model_.reset_test();
  init();
}

LcdTestPage::~LcdTestPage() {
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
  prompt_ = lv_label_create(content);
  lv_label_set_text(prompt_, "Press 6 or Enter to start display test");
  lv_obj_set_width(prompt_, 280);
  lv_obj_set_style_text_align(prompt_, LV_TEXT_ALIGN_CENTER, 0);
  auto* prompt_font = assets_ref_().load_font("inter-medium.ttf", 16);
  lv_obj_set_style_text_font(prompt_, prompt_font ? prompt_font : &lv_font_montserrat_16, 0);
  lv_obj_center(prompt_);
  reactive::bind_theme(prompt_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  brightness_group_ = lv_obj_create(content);
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
  lv_obj_add_flag(brightness_group_, LV_OBJ_FLAG_HIDDEN);

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
  lv_slider_set_range(brightness_slider_, 0, 100);
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
}

void LcdTestPage::apply_color_index_(int32_t index) {
  if (!prompt_ || !color_layer_ || !brightness_group_) {
    return;
  }

  if (lcd_view_model_.is_brightness_test_active()) {
    lv_obj_add_flag(prompt_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  if (index <= 0) {
    lv_label_set_text(prompt_, "Press 6 or Enter to start display test");
    lv_obj_remove_flag(prompt_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(brightness_group_, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  if (index > model::LcdTestModel::K_COLOR_STEP_COUNT) {
    lv_label_set_text(prompt_,
                      "Did all display colors display correctly?\nPress 6 or Enter to continue.");
    lv_obj_remove_flag(prompt_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(brightness_group_, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  const auto& steps                  = lcd_color_steps();
  const auto& step                   = steps[static_cast<std::size_t>(index - 1)];
  const bool was_color_layer_visible = !lv_obj_has_flag(color_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(prompt_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(brightness_group_, LV_OBJ_FLAG_HIDDEN);
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
    lv_obj_remove_flag(brightness_group_, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_add_flag(brightness_group_, LV_OBJ_FLAG_HIDDEN);
}

void LcdTestPage::apply_brightness_percent_(int32_t percent) {
  requested_brightness_percent_ = std::clamp(percent, 0, 100);
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
  target_brightness_percent_ = std::clamp(requested_brightness_percent_, 0, 100);

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
  hardware_brightness_percent_ = std::clamp(hardware_brightness_percent_ + step, 0, 100);
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
