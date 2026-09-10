/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "fire_in_the_pi_page.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "logger.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

struct CookingStep {
  const char* text;
  uint32_t background;
  uint32_t foreground;
};

constexpr std::array<CookingStep, 6> K_COOKING_STEPS = {{
    {"Prepare the Pie Crust...", 0xD99B4A, 0x24170B},
    {"Making the Filling...", 0xA83255, 0xFFFFFF},
    {"Assembling the Pie...", 0x6F8F45, 0xFFFFFF},
    {"Baking the Pie...", 0xC55224, 0xFFFFFF},
    {"Cooling your Pie...", 0x287F87, 0xFFFFFF},
    {"Serving the Pie...", 0x6F497D, 0xFFFFFF},
}};

using MetricMember = std::optional<double> platform::baking::BakingMetricSample::*;

struct ChartConfig {
  const char* title;
  const char* unit;
  MetricMember member;
  double chart_scale;
  int32_t minimum;
  int32_t maximum;
  bool dynamic_range;
  int value_precision;
};

constexpr ChartConfig K_CHART_CONFIGS[] = {
    {"SoC Temp",
     "C",
     &platform::baking::BakingMetricSample::soc_temperature_c,
     10.0,
     0,
     1200,
     false,
     1},
    {"Battery Temp",
     "C",
     &platform::baking::BakingMetricSample::battery_temperature_c,
     10.0,
     0,
     800,
     false,
     1},
    {"Voltage",
     "V",
     &platform::baking::BakingMetricSample::battery_voltage_v,
     1000.0,
     2500,
     5000,
     false,
     2},
    {"Current",
     "A",
     &platform::baking::BakingMetricSample::battery_current_a,
     1000.0,
     -3000,
     3000,
     true,
     2},
    {"CPU Load",
     "%",
     &platform::baking::BakingMetricSample::cpu_usage_percent,
     10.0,
     0,
     1000,
     false,
     1},
    {"Memory",
     "%",
     &platform::baking::BakingMetricSample::memory_usage_percent,
     10.0,
     0,
     1000,
     false,
     1},
    {"SD Usage",
     "%",
     &platform::baking::BakingMetricSample::storage_usage_percent,
     10.0,
     0,
     1000,
     false,
     1},
    {"SD Read",
     "KiB/s",
     &platform::baking::BakingMetricSample::storage_read_kib_per_second,
     1.0,
     0,
     1024,
     true,
     0},
    {"SD Write",
     "KiB/s",
     &platform::baking::BakingMetricSample::storage_write_kib_per_second,
     1.0,
     0,
     1024,
     true,
     0},
};

constexpr uint32_t K_STEP_DURATION_MS  = 1800;
constexpr uint32_t K_UI_UPDATE_MS      = 1000;
constexpr uint32_t K_CHART_POINT_COUNT = 120;

bool is_enter_key(uint32_t key, const char* key_name) {
  return key == LV_KEY_ENTER || key == '\n' || key == '\r' || key == '8' ||
         (key_name &&
          (std::strcmp(key_name, "Enter") == 0 || std::strcmp(key_name, "Return") == 0));
}

bool is_back_key(uint32_t key, const char* key_name) {
  return key == LV_KEY_ESC || key == 27 || key == '4' ||
         (key_name && (std::strcmp(key_name, "Esc") == 0 || std::strcmp(key_name, "Escape") == 0));
}

bool is_left_key(uint32_t key) { return key == LV_KEY_LEFT || key == 'z' || key == 'Z'; }

bool is_right_key(uint32_t key) {
  return key == LV_KEY_RIGHT || key == 'c' || key == 'C' || key == 'x' || key == 'X';
}

std::string format_elapsed(int elapsed_seconds) {
  const auto minutes = std::max(0, elapsed_seconds) / 60;
  const auto seconds = std::max(0, elapsed_seconds) % 60;
  std::ostringstream text;
  text << std::setfill('0') << std::setw(2) << minutes << ':' << std::setw(2) << seconds;
  return text.str();
}

int32_t chart_value(const std::optional<double>& value, double scale) {
  if (!value || !std::isfinite(*value)) {
    return LV_CHART_POINT_NONE;
  }
  const auto scaled = std::round(*value * scale);
  if (scaled > static_cast<double>(INT32_MAX - 1) || scaled < static_cast<double>(INT32_MIN)) {
    return LV_CHART_POINT_NONE;
  }
  return static_cast<int32_t>(scaled);
}

}  // namespace

FireInThePiPage::FireInThePiPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  baking_service_ = std::make_unique<platform::baking::BakingPiService>();
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  set_default_test_nav_(false);
  init();
  platform::set_key_listener(key_listener, this);
}

FireInThePiPage::~FireInThePiPage() {
  platform::clear_key_listener(key_listener, this);
  if (baking_service_) {
    baking_service_->stop();
  }
  if (step_timer_) {
    lv_timer_delete(step_timer_);
    step_timer_ = nullptr;
  }
  if (ui_timer_) {
    lv_timer_delete(ui_timer_);
    ui_timer_ = nullptr;
  }
  if (cooking_layer_) {
    lv_anim_delete(cooking_layer_, nullptr);
  }
  if (status_label_) {
    lv_anim_delete(status_label_, nullptr);
  }
}

void FireInThePiPage::build_content(lv_obj_t* content) {
  chart_ = lv_chart_create(content);
  lv_obj_set_size(chart_, 300, 88);
  lv_obj_align(chart_, LV_ALIGN_BOTTOM_MID, 0, -3);
  lv_chart_set_type(chart_, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart_, K_CHART_POINT_COUNT);
  lv_chart_set_update_mode(chart_, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_div_line_count(chart_, 4, 6);
  chart_series_ = lv_chart_add_series(chart_,
                                      view::palette(app_view_model_ref_().is_dark_mode()).info,
                                      LV_CHART_AXIS_PRIMARY_Y);
  view::apply_chart_theme(chart_, chart_series_, app_view_model_ref_().is_dark_mode());
  lv_obj_set_style_size(chart_, 0, 0, LV_PART_INDICATOR);

  chart_value_label_ = lv_label_create(content);
  lv_obj_set_width(chart_value_label_, 150);
  lv_obj_align(chart_value_label_, LV_ALIGN_TOP_RIGHT, -8, 0);
  lv_obj_set_style_text_align(chart_value_label_, LV_TEXT_ALIGN_RIGHT, 0);
  auto* value_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 12);
  lv_obj_set_style_text_font(chart_value_label_,
                             value_font ? value_font : &lv_font_montserrat_12,
                             0);
  reactive::bind_theme(chart_value_label_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  auto* screen_root = lv_obj_get_parent(content);
  cooking_layer_    = lv_obj_create(screen_root);
  lv_obj_remove_style_all(cooking_layer_);
  lv_obj_set_size(cooking_layer_, view::K_SCREEN_WIDTH, view::K_SCREEN_HEIGHT);
  lv_obj_align(cooking_layer_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(cooking_layer_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(cooking_layer_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(cooking_layer_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(cooking_layer_, LV_OBJ_FLAG_HIDDEN);

  status_label_ = lv_label_create(cooking_layer_);
  lv_obj_set_width(status_label_, 280);
  lv_label_set_long_mode(status_label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(status_label_);

  cycle_label_ = lv_label_create(cooking_layer_);
  lv_obj_set_width(cycle_label_, 280);
  lv_obj_set_style_text_align(cycle_label_, LV_TEXT_ALIGN_CENTER, 0);
  auto* cycle_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 12);
  lv_obj_set_style_text_font(cycle_label_, cycle_font ? cycle_font : &lv_font_montserrat_12, 0);
  lv_obj_align(cycle_label_, LV_ALIGN_BOTTOM_MID, 0, -8);

  step_timer_ = lv_timer_create(step_timer_cb, K_STEP_DURATION_MS, this);
  lv_timer_pause(step_timer_);
  ui_timer_ = lv_timer_create(ui_timer_cb, K_UI_UPDATE_MS, this);
  start_cooking_();
}

void FireInThePiPage::start_cooking_() {
  if (cooking_ || !cooking_layer_ || !step_timer_) {
    return;
  }

  cooking_      = true;
  current_step_ = 0;
  show_step_(current_step_, false);
  lv_obj_remove_flag(cooking_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(cooking_layer_);
  lv_timer_reset(step_timer_);
  lv_timer_resume(step_timer_);
  update_nav_();
  if (baking_service_ && !baking_service_->start()) {
    LOG_WARN("[BAKING] page entered but service did not start");
  }
}

void FireInThePiPage::show_step_(std::size_t index, bool animate) {
  if (!cooking_layer_ || !status_label_ || !cycle_label_ || index >= K_COOKING_STEPS.size()) {
    return;
  }

  const auto& step = K_COOKING_STEPS[index];
  const auto text  = app_view_model_ref_().tr(step.text);
  lv_label_set_text(status_label_, text.c_str());
  auto* status_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-semibold.ttf"), 24);
  lv_obj_set_style_text_font(status_label_, status_font ? status_font : &lv_font_montserrat_24, 0);
  lv_obj_center(status_label_);

  const auto background = lv_color_hex(step.background);
  const auto foreground = lv_color_hex(step.foreground);
  if (animate) {
    view::animate_style_color(cooking_layer_, LV_STYLE_BG_COLOR, background, 520);
    view::animate_style_color(status_label_, LV_STYLE_TEXT_COLOR, foreground, 320);
    view::animate_style_color(cycle_label_, LV_STYLE_TEXT_COLOR, foreground, 320);
  } else {
    lv_obj_set_style_bg_color(cooking_layer_, background, 0);
    lv_obj_set_style_text_color(status_label_, foreground, 0);
    lv_obj_set_style_text_color(cycle_label_, foreground, 0);
  }
}

void FireInThePiPage::show_chart_() {
  if (chart_visible_ || !cooking_) {
    return;
  }
  chart_visible_ = true;
  lv_obj_add_flag(cooking_layer_, LV_OBJ_FLAG_HIDDEN);
  if (step_timer_) {
    lv_timer_pause(step_timer_);
  }
  update_nav_();
  refresh_chart_();
}

void FireInThePiPage::hide_chart_() {
  if (!chart_visible_) {
    return;
  }
  chart_visible_ = false;
  app_view_model_ref_().set_title_text("Fire in the Pi");
  if (auto* title_bar = title_bar_ref_()) {
    title_bar->set_title_alignment(LV_ALIGN_LEFT_MID, 8, 0);
  }
  lv_obj_remove_flag(cooking_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(cooking_layer_);
  if (step_timer_) {
    lv_timer_reset(step_timer_);
    lv_timer_resume(step_timer_);
  }
  update_runtime_ui_();
  update_nav_();
}

void FireInThePiPage::change_chart_(int direction) {
  const auto count = std::size(K_CHART_CONFIGS);
  auto index       = static_cast<std::size_t>(chart_metric_);
  if (direction < 0) {
    index = (index + count - 1) % count;
  } else {
    index = (index + 1) % count;
  }
  chart_metric_ = static_cast<ChartMetric>(index);
  refresh_chart_();
}

void FireInThePiPage::update_nav_() {
  app_view_model_ref_().clear_nav_actions();
  set_nav_action_('4', view::ICON_ARROW_U_UP_LEFT, [this]() {
    if (chart_visible_) {
      hide_chart_();
    } else {
      app_view_model_ref_().request_back_or_quit();
    }
  });
}

void FireInThePiPage::update_runtime_ui_() {
  if (!baking_service_ || !cycle_label_ || !status_label_) {
    return;
  }
  const auto snapshot   = baking_service_->snapshot();
  const auto cycle_text = app_view_model_ref_().tr("Cycle") + " " + std::to_string(snapshot.cycle) +
                          " | " + format_elapsed(snapshot.elapsed_seconds);
  lv_label_set_text(cycle_label_, cycle_text.c_str());

  if (!snapshot.error_message.empty()) {
    if (step_timer_) {
      lv_timer_pause(step_timer_);
    }
    const auto colors = view::palette(app_view_model_ref_().is_dark_mode());
    lv_obj_set_style_bg_color(cooking_layer_, colors.surface, 0);
    lv_obj_set_style_text_color(status_label_, colors.error, 0);
    lv_obj_set_style_text_color(cycle_label_, colors.text_muted, 0);
    auto* error_font =
        assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-semibold.ttf"), 14);
    lv_obj_set_style_text_font(status_label_, error_font ? error_font : &lv_font_montserrat_14, 0);
    lv_label_set_text(status_label_, snapshot.error_message.c_str());
    lv_obj_center(status_label_);
  }
}

void FireInThePiPage::refresh_chart_() {
  if (!chart_ || !chart_series_ || !chart_value_label_ || !baking_service_) {
    return;
  }
  const auto index   = static_cast<std::size_t>(chart_metric_);
  const auto& config = K_CHART_CONFIGS[index];
  const auto history = baking_service_->metric_history();
  view::apply_chart_theme(chart_, chart_series_, app_view_model_ref_().is_dark_mode());

  int32_t range_min = config.minimum;
  int32_t range_max = config.maximum;
  if (config.dynamic_range) {
    bool have_value  = false;
    int32_t data_min = 0;
    int32_t data_max = 0;
    for (const auto& sample : history) {
      const auto value = chart_value(sample.*(config.member), config.chart_scale);
      if (value == LV_CHART_POINT_NONE) {
        continue;
      }
      if (!have_value) {
        data_min   = value;
        data_max   = value;
        have_value = true;
      } else {
        data_min = std::min(data_min, value);
        data_max = std::max(data_max, value);
      }
    }
    if (have_value) {
      data_min        = std::min(0, data_min);
      data_max        = std::max(0, data_max);
      const auto span = std::max<int32_t>(100, data_max - data_min);
      range_min       = data_min - span / 10;
      range_max       = data_max + span / 10;
    }
  }
  if (range_min == range_max) {
    ++range_max;
  }
  lv_chart_set_axis_range(chart_, LV_CHART_AXIS_PRIMARY_Y, range_min, range_max);
  lv_chart_set_all_values(chart_, chart_series_, LV_CHART_POINT_NONE);
  for (const auto& sample : history) {
    lv_chart_set_next_value(chart_,
                            chart_series_,
                            chart_value(sample.*(config.member), config.chart_scale));
  }

  std::string value_text = "--";
  if (!history.empty()) {
    const auto& latest = history.back().*(config.member);
    if (latest && std::isfinite(*latest)) {
      std::ostringstream output;
      output << std::fixed << std::setprecision(config.value_precision) << *latest << ' '
             << config.unit;
      value_text = output.str();
    }
  }
  lv_label_set_text(chart_value_label_, value_text.c_str());
  app_view_model_ref_().set_title_text(config.title);
  if (auto* title_bar = title_bar_ref_()) {
    title_bar->set_title_alignment(LV_ALIGN_LEFT_MID, 8, 0);
  }
  lv_obj_invalidate(chart_);
}

void FireInThePiPage::step_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<FireInThePiPage*>(lv_timer_get_user_data(timer));
  if (!page || !page->cooking_ || page->chart_visible_) {
    return;
  }

  page->current_step_ = (page->current_step_ + 1) % K_COOKING_STEPS.size();
  page->show_step_(page->current_step_, true);
}

void FireInThePiPage::ui_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<FireInThePiPage*>(lv_timer_get_user_data(timer));
  if (!page || !page->cooking_) {
    return;
  }
  page->update_runtime_ui_();
  if (page->chart_visible_) {
    page->refresh_chart_();
  }
}

void FireInThePiPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<FireInThePiPage*>(user_data);
  if (!page) {
    return;
  }

  if (is_back_key(key, key_name)) {
    // ESC/4 is dispatched to the currently configured navbar back action.
    return;
  }
  if (key == 's' || key == 'S') {
    page->show_chart_();
    return;
  }
  if (page->chart_visible_ && is_left_key(key)) {
    page->change_chart_(-1);
    return;
  }
  if (page->chart_visible_ && is_right_key(key)) {
    page->change_chart_(1);
    return;
  }
  if (is_enter_key(key, key_name)) {
    page->start_cooking_();
  }
}

}  // namespace screen
