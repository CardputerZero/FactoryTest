/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "imu_test_page.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <sstream>
#include <string>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

struct AxisRow {
  const char* icon;
  const char* label;
  const char* unit;
};

constexpr int32_t K_VIEWPORT_WIDTH  = view::K_SCREEN_WIDTH;
constexpr int32_t K_VIEWPORT_HEIGHT = 106;
constexpr int32_t K_GRID_WIDTH      = 300;
constexpr int32_t K_CARD_WIDTH      = 300;
constexpr int32_t K_CARD_HEIGHT     = 34;
constexpr int32_t K_SCROLL_STEP     = 40;
constexpr int32_t K_BOUNCE_OFFSET   = 14;
constexpr uint32_t K_BOUNCE_BACK_MS = 120;

constexpr std::array<AxisRow, 9> K_AXIS_ROWS = {{{view::ICON_VECTOR_THREE, "Accel X", "m/s^2"},
                                                 {view::ICON_VECTOR_THREE, "Accel Y", "m/s^2"},
                                                 {view::ICON_VECTOR_THREE, "Accel Z", "m/s^2"},
                                                 {view::ICON_REPEAT, "Gyro X", "rad/s"},
                                                 {view::ICON_REPEAT, "Gyro Y", "rad/s"},
                                                 {view::ICON_REPEAT, "Gyro Z", "rad/s"},
                                                 {view::ICON_SCAN, "Mag X", "G"},
                                                 {view::ICON_SCAN, "Mag Y", "G"},
                                                 {view::ICON_SCAN, "Mag Z", "G"}}};

std::string sensor_label(const std::string& name, const std::string& path) {
  const auto device_node = std::filesystem::path(path).filename().string();
  if (name.empty()) {
    return device_node;
  }
  if (device_node.empty()) {
    return name;
  }
  return name + " (" + device_node + ")";
}

}  // namespace

ImuTestPage::ImuTestPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  set_default_test_nav_();
  has_imu_ = platform::imu::find_bmi270_device(device_, status_message_);
  if (has_imu_) {
    std::ostringstream status;
    if (device_.has_bmi270) {
      status << sensor_label(device_.display_name, device_.iio_path);
    } else {
      status << app_view_model_ref_().tr("BMI270 not found");
    }
    status << " + ";
    if (device_.has_bmm150) {
      status << sensor_label(device_.mag_display_name, device_.mag_iio_path);
    } else {
      status << app_view_model_ref_().tr("BMM150 not found");
    }
    status_message_ = status.str();
  }
  init();
  platform::set_key_listener(key_listener, this);
  poll_timer_ = lv_timer_create(poll_timer_cb, 200, this);
}

ImuTestPage::~ImuTestPage() {
  platform::clear_key_listener(key_listener, this);
  if (poll_timer_) {
    lv_timer_delete(poll_timer_);
    poll_timer_ = nullptr;
  }
  if (scroll_bounce_timer_) {
    lv_timer_delete(scroll_bounce_timer_);
    scroll_bounce_timer_ = nullptr;
  }
}

void ImuTestPage::build_content(lv_obj_t* content) {
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

  status_label_ = lv_label_create(grid_);
  lv_obj_set_width(status_label_, 290);
  lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL);
  auto* status_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 11);
  lv_obj_set_style_text_font(status_label_, status_font ? status_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(status_label_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);
  lv_label_set_text(status_label_, status_message_.c_str());

  auto* icon_font  = assets_ref_().load_font("Phosphor-Fill.ttf", 14);
  auto* label_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-semibold.ttf"), 11);
  auto* value_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 11);

  for (std::size_t i = 0; i < K_AXIS_ROWS.size(); ++i) {
    const auto label = app_view_model_ref_().tr(K_AXIS_ROWS[i].label);
    cards_[i] =
        std::make_unique<view::widgets::IconCard>(grid_,
                                                  app_view_model_ref_(),
                                                  K_AXIS_ROWS[i].icon,
                                                  label.c_str(),
                                                  "--",
                                                  icon_font ? icon_font : &lv_font_montserrat_14,
                                                  label_font ? label_font : &lv_font_montserrat_12,
                                                  value_font ? value_font : &lv_font_montserrat_12,
                                                  K_CARD_WIDTH,
                                                  K_CARD_HEIGHT);
    cards_[i]->build();
  }

  if (!has_imu_) {
    for (auto& card : cards_) {
      if (card && card->root()) {
        lv_obj_add_flag(card->root(), LV_OBJ_FLAG_HIDDEN);
      }
    }
  } else {
    update_readings_();
  }
}

void ImuTestPage::update_readings_() {
  if (!has_imu_ || !grid_ || !status_label_) {
    return;
  }

  platform::imu::NineAxisReading reading;
  std::string error;
  if (!platform::imu::read_nine_axis(device_, reading, error)) {
    lv_label_set_text(status_label_, error.c_str());
    return;
  }

  lv_label_set_text(status_label_, status_message_.c_str());
  if (device_.has_bmi270) {
    set_value_(0, reading.accel_x, K_AXIS_ROWS[0].unit);
    set_value_(1, reading.accel_y, K_AXIS_ROWS[1].unit);
    set_value_(2, reading.accel_z, K_AXIS_ROWS[2].unit);
    set_value_(3, reading.gyro_x, K_AXIS_ROWS[3].unit);
    set_value_(4, reading.gyro_y, K_AXIS_ROWS[4].unit);
    set_value_(5, reading.gyro_z, K_AXIS_ROWS[5].unit);
  } else {
    for (std::size_t i = 0; i < 6; ++i) {
      set_missing_(i, app_view_model_ref_().tr("BMI270 not found").c_str());
    }
  }

  if (device_.has_bmm150) {
    set_value_(6, reading.magn_x, K_AXIS_ROWS[6].unit);
    set_value_(7, reading.magn_y, K_AXIS_ROWS[7].unit);
    set_value_(8, reading.magn_z, K_AXIS_ROWS[8].unit);
  } else {
    for (std::size_t i = 6; i < 9; ++i) {
      set_missing_(i, app_view_model_ref_().tr("BMM150 not found").c_str());
    }
  }
}

void ImuTestPage::set_value_(std::size_t index, double value, const char* unit) {
  if (index >= cards_.size() || !cards_[index]) {
    return;
  }

  char buffer[48];
  lv_snprintf(buffer, sizeof(buffer), "%.3f %s", value, unit);
  cards_[index]->set_value(buffer);
}

void ImuTestPage::set_missing_(std::size_t index, const char* message) {
  if (index >= cards_.size() || !cards_[index]) {
    return;
  }
  const auto text = app_view_model_ref_().tr(message ? message : "Not found");
  cards_[index]->set_value(text.c_str());
}

void ImuTestPage::scroll_(int32_t direction) {
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

void ImuTestPage::poll_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<ImuTestPage*>(lv_timer_get_user_data(timer));
  if (page) {
    page->update_readings_();
  }
}

void ImuTestPage::scroll_bounce_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<ImuTestPage*>(lv_timer_get_user_data(timer));
  if (!page || !page->viewport_) {
    return;
  }

  lv_obj_scroll_to_y(page->viewport_, page->scroll_bounce_target_y_, LV_ANIM_ON);
}

void ImuTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<ImuTestPage*>(user_data);
  if (!page) {
    return;
  }
  if (page->handle_test_result_dialog_key_(key, key_name)) {
    return;
  }

  switch (key) {
    case LV_KEY_UP:
    case '5':
    case 'f':
    case 'F':
      page->scroll_(-1);
      break;
    case LV_KEY_DOWN:
    case '7':
    case 'x':
    case 'X':
      page->scroll_(1);
      break;
    default:
      break;
  }
}

}  // namespace screen
