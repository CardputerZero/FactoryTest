/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "power_info_page.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "ui_const.h"

namespace screen {
namespace {

struct PowerField {
  const char* label;
  std::string value;
};

constexpr int32_t K_GRID_WIDTH               = 300;
constexpr int32_t K_VIEWPORT_WIDTH           = 320;
constexpr int32_t K_VIEWPORT_HEIGHT          = 106;
constexpr int32_t K_CARD_WIDTH               = 300;
constexpr int32_t K_CARD_HEIGHT              = 34;
constexpr int32_t K_SCROLL_STEP              = 40;
constexpr int32_t K_BOUNCE_OFFSET            = 14;
constexpr uint32_t K_REFRESH_MS              = 2000;
constexpr uint32_t K_BOUNCE_BACK_MS          = 120;
constexpr const char* K_EMPTY_VALUE          = "--";
constexpr std::size_t K_CAPACITY_FIELD_INDEX = 2;

const char* battery_icon_for_capacity(int32_t capacity_percent) {
  if (capacity_percent >= 95) {
    return view::ICON_BAT_FULL;
  }
  if (capacity_percent >= 75) {
    return view::ICON_BAT_HIGH;
  }
  if (capacity_percent >= 50) {
    return view::ICON_BAT_MEDIUM;
  }
  if (capacity_percent >= 20) {
    return view::ICON_BAT_LOW;
  }
  return view::ICON_BAT_EMPTY;
}

const char* field_icon(std::size_t index, int32_t capacity_percent) {
  static constexpr const char* ICONS[] = {
      view::ICON_CPU,
      view::ICON_INFO,
      nullptr,
      view::ICON_LIGHTNING,
      view::ICON_GAUGE,
      view::ICON_PULSE,
      view::ICON_HEART,
      view::ICON_THERMOMETER,
      view::ICON_REPEAT,
      view::ICON_TAG,
  };
  if (index == K_CAPACITY_FIELD_INDEX) {
    return battery_icon_for_capacity(capacity_percent);
  }
  return index < (sizeof(ICONS) / sizeof(ICONS[0])) && ICONS[index] ? ICONS[index]
                                                                    : view::ICON_INFO;
}

std::string text_or_empty(const std::string& value) {
  return value.empty() ? K_EMPTY_VALUE : value;
}

std::string format_int(int32_t value) {
  if (value < 0) {
    return K_EMPTY_VALUE;
  }
  return std::to_string(value);
}

std::string format_percent(int32_t value) {
  if (value < 0) {
    return K_EMPTY_VALUE;
  }
  return std::to_string(value) + "%";
}

std::string format_voltage(int64_t microvolts) {
  if (microvolts < 0) {
    return K_EMPTY_VALUE;
  }

  char buffer[24];
  std::snprintf(buffer, sizeof(buffer), "%.2fV", static_cast<double>(microvolts) / 1000000.0);
  return buffer;
}

std::string format_current(int64_t microamps) {
  if (microamps == -1) {
    return K_EMPTY_VALUE;
  }

  char buffer[24];
  std::snprintf(buffer, sizeof(buffer), "%.0fmA", static_cast<double>(microamps) / 1000.0);
  return buffer;
}

std::string format_charge(const platform::power::PowerSupplyInfo& info) {
  if (info.charge_now_uah < 0) {
    return K_EMPTY_VALUE;
  }

  const auto now_mah = static_cast<int>(info.charge_now_uah / 1000);
  if (info.charge_full_uah < 0) {
    return std::to_string(now_mah) + "mAh";
  }

  const auto full_mah = static_cast<int>(info.charge_full_uah / 1000);
  return std::to_string(now_mah) + "/" + std::to_string(full_mah);
}

std::string format_temp(int32_t decic) {
  if (decic <= -100000) {
    return K_EMPTY_VALUE;
  }

  char buffer[24];
  std::snprintf(buffer, sizeof(buffer), "%.1fC", static_cast<double>(decic) / 10.0);
  return buffer;
}

std::array<PowerField, PowerInfoPage::K_FIELD_COUNT> make_fields(
    const platform::power::PowerSupplyInfo& info) {
  const int64_t voltage =
      info.voltage_instant_uv >= 0 ? info.voltage_instant_uv : info.voltage_now_uv;
  const int64_t current =
      info.current_instant_ua >= 0 ? info.current_instant_ua : info.current_now_ua;

  return {{
      {"Device", text_or_empty(info.device_name)},
      {"Status", info.present == 0 ? "Not present" : text_or_empty(info.status)},
      {"Capacity", format_percent(info.capacity_percent)},
      {"Voltage", format_voltage(voltage)},
      {"Charge", format_charge(info)},
      {"Current", format_current(current)},
      {"Health", text_or_empty(info.health)},
      {"Temp", format_temp(info.temp_decic)},
      {"Cycle", format_int(info.cycle_count)},
      {"Tech", text_or_empty(info.technology)},
  }};
}

std::array<PowerField, PowerInfoPage::K_FIELD_COUNT> make_error_fields(
    const std::string& error_message) {
  return {{
      {"Device", "Unavailable"},
      {"Status", error_message.empty() ? "No battery" : error_message},
      {"Capacity", K_EMPTY_VALUE},
      {"Voltage", K_EMPTY_VALUE},
      {"Charge", K_EMPTY_VALUE},
      {"Current", K_EMPTY_VALUE},
      {"Health", K_EMPTY_VALUE},
      {"Temp", K_EMPTY_VALUE},
      {"Cycle", K_EMPTY_VALUE},
      {"Tech", K_EMPTY_VALUE},
  }};
}

}  // namespace

PowerInfoPage::PowerInfoPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  set_default_test_nav_();
  init();
  platform::set_key_listener(key_listener, this);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_MS, this);
}

PowerInfoPage::~PowerInfoPage() {
  platform::clear_key_listener(key_listener, this);
  if (refresh_timer_) {
    lv_timer_delete(refresh_timer_);
    refresh_timer_ = nullptr;
  }
  if (scroll_bounce_timer_) {
    lv_timer_delete(scroll_bounce_timer_);
    scroll_bounce_timer_ = nullptr;
  }
}

void PowerInfoPage::build_content(lv_obj_t* content) {
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
  lv_obj_set_width(grid_, K_GRID_WIDTH);
  lv_obj_set_height(grid_, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grid_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(grid_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(grid_, 6, 0);
  lv_obj_set_style_pad_top(grid_, 2, 0);
  lv_obj_set_style_pad_bottom(grid_, 4, 0);
  lv_obj_clear_flag(grid_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(grid_, LV_ALIGN_TOP_MID, 0, 0);

  auto* icon_font  = assets_ref_().load_font("Phosphor-Fill.ttf", 14);
  auto* key_font   = assets_ref_().load_font("inter-semibold.ttf", 11);
  auto* value_font = assets_ref_().load_font("inter-medium.ttf", 11);
  platform::power::PowerSupplyInfo info;
  std::string error_message;
  const bool has_info            = platform::power::read_battery_info(info, error_message);
  const auto fields              = has_info ? make_fields(info) : make_error_fields(error_message);
  const int32_t capacity_percent = has_info ? info.capacity_percent : -1;

  for (std::size_t i = 0; i < fields.size(); ++i) {
    cards_[i] =
        std::make_unique<view::widgets::IconCard>(grid_,
                                                  app_view_model_ref_(),
                                                  field_icon(i, capacity_percent),
                                                  fields[i].label,
                                                  fields[i].value.c_str(),
                                                  icon_font ? icon_font : &lv_font_montserrat_14,
                                                  key_font ? key_font : &lv_font_montserrat_12,
                                                  value_font ? value_font : &lv_font_montserrat_12,
                                                  K_CARD_WIDTH,
                                                  K_CARD_HEIGHT);
    cards_[i]->build();
  }
}

void PowerInfoPage::scroll_(int32_t direction) {
  if (!viewport_ || direction == 0) {
    return;
  }

  lv_obj_update_layout(viewport_);
  const int32_t current_y = lv_obj_get_scroll_y(viewport_);
  const int32_t max_y     = current_y + lv_obj_get_scroll_bottom(viewport_);

  if (direction < 0 && current_y <= 0) {
    scroll_bounce_target_y_ = 0;
    lv_obj_scroll_by(viewport_, 0, K_BOUNCE_OFFSET, LV_ANIM_ON);
  } else if (direction > 0 && current_y >= max_y) {
    scroll_bounce_target_y_ = max_y;
    lv_obj_scroll_by(viewport_, 0, -K_BOUNCE_OFFSET, LV_ANIM_ON);
  } else {
    const int32_t target_y = std::clamp(current_y + direction * K_SCROLL_STEP, 0, max_y);
    lv_obj_scroll_to_y(viewport_, target_y, LV_ANIM_ON);
    return;
  }

  if (!scroll_bounce_timer_) {
    scroll_bounce_timer_ = lv_timer_create(scroll_bounce_timer_cb, K_BOUNCE_BACK_MS, this);
    lv_timer_set_auto_delete(scroll_bounce_timer_, false);
  }
  lv_timer_set_repeat_count(scroll_bounce_timer_, 1);
  lv_timer_resume(scroll_bounce_timer_);
  lv_timer_reset(scroll_bounce_timer_);
}

void PowerInfoPage::refresh_() {
  platform::power::PowerSupplyInfo info;
  std::string error_message;
  const bool has_info            = platform::power::read_battery_info(info, error_message);
  const auto fields              = has_info ? make_fields(info) : make_error_fields(error_message);
  const int32_t capacity_percent = has_info ? info.capacity_percent : -1;

  for (std::size_t i = 0; i < cards_.size(); ++i) {
    if (cards_[i]) {
      if (i == K_CAPACITY_FIELD_INDEX) {
        cards_[i]->set_icon(field_icon(i, capacity_percent));
      }
      cards_[i]->set_value(fields[i].value.c_str());
    }
  }
}

void PowerInfoPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  LV_UNUSED(key_name);

  auto* page = static_cast<PowerInfoPage*>(user_data);
  if (!page) {
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

void PowerInfoPage::scroll_bounce_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<PowerInfoPage*>(lv_timer_get_user_data(timer));
  if (!page || !page->viewport_) {
    return;
  }

  lv_obj_scroll_to_y(page->viewport_, page->scroll_bounce_target_y_, LV_ANIM_ON);
}

void PowerInfoPage::refresh_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<PowerInfoPage*>(lv_timer_get_user_data(timer));
  if (!page) {
    return;
  }

  page->refresh_();
}

}  // namespace screen
