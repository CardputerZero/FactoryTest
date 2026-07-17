/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "device_info_page.h"

#include <algorithm>
#include <cstddef>
#include <memory>

#include "asset_manager.h"
#include "bindings.h"
#include "device_info_service.h"
#include "linux_input.h"
#include "ui_const.h"
#include "version.h"

namespace screen {
namespace {

constexpr int32_t K_VIEWPORT_WIDTH  = 320;
constexpr int32_t K_VIEWPORT_HEIGHT = 106;
constexpr int32_t K_GRID_WIDTH      = 300;
constexpr int32_t K_CARD_WIDTH      = 300;
constexpr int32_t K_CARD_HEIGHT     = 34;
constexpr int32_t K_TITLE_WIDTH     = 116;
constexpr int32_t K_SCROLL_STEP     = 40;
constexpr int32_t K_BOUNCE_OFFSET   = 14;
constexpr uint32_t K_REFRESH_MS     = 5000;
constexpr uint32_t K_BOUNCE_BACK_MS = 120;
constexpr const char* K_EMPTY_VALUE = "--";

const char* field_icon(std::size_t index) {
  static constexpr const char* ICONS[] = {
      view::ICON_DEVICE_MOBILE,
      view::ICON_WRENCH,
      view::ICON_CIRCUIT,
      view::ICON_FINGERPRINT,
      view::ICON_MEMORY,
      view::ICON_HARDDRIVE,
      view::ICON_GLOBE,
      view::ICON_LINUX_LOGO,
      view::ICON_PACKAGE,
      view::ICON_TIMER,
      view::ICON_CLOCK,
      view::ICON_MAPPIN,
      view::ICON_GIT_BRANCH,
      view::ICON_WRENCH,
  };
  return index < (sizeof(ICONS) / sizeof(ICONS[0])) ? ICONS[index] : view::ICON_INFO;
}

}  // namespace

DeviceInfoPage::DeviceInfoPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  set_default_test_nav_();
  init();
  platform::set_key_listener(key_listener, this);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_MS, this);
}

DeviceInfoPage::~DeviceInfoPage() {
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

void DeviceInfoPage::build_content(lv_obj_t* content) {
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

  auto* icon_font   = assets_ref_().load_font("Phosphor-Fill.ttf", 14);
  auto* key_font    = assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-semibold.ttf"), 11);
  auto* value_font  = assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 11);
  auto fields = platform::device_info::read_device_info_fields();
  fields.push_back({"Software Version", factory_test::get_version_str()});
  fields.push_back({"Build time", factory_test::get_build_time_str()});
  cards_.clear();
  cards_.reserve(fields.size());

  for (std::size_t i = 0; i < fields.size(); ++i) {
    const auto label = app_view_model_ref_().tr(fields[i].label.c_str());
    auto card = std::make_unique<view::widgets::IconCard>(
        grid_,
        app_view_model_ref_(),
        field_icon(i),
        label.c_str(),
        fields[i].value.empty() ? K_EMPTY_VALUE : fields[i].value.c_str(),
        icon_font ? icon_font : &lv_font_montserrat_14,
        key_font ? key_font : &lv_font_montserrat_12,
        value_font ? value_font : &lv_font_montserrat_12,
        K_CARD_WIDTH,
        K_CARD_HEIGHT,
        LV_LABEL_LONG_SCROLL,
        K_TITLE_WIDTH);
    card->build();
    cards_.push_back(std::move(card));
  }
}

void DeviceInfoPage::scroll_(int32_t direction) {
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

void DeviceInfoPage::refresh_() {
  const auto fields = platform::device_info::read_device_info_fields();
  for (std::size_t i = 0; i < fields.size() && i < cards_.size(); ++i) {
    if (cards_[i]) {
      cards_[i]->set_value(fields[i].value.empty() ? K_EMPTY_VALUE : fields[i].value.c_str());
    }
  }
}

void DeviceInfoPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<DeviceInfoPage*>(user_data);
  if (!page) {
    return;
  }
  if (page->handle_test_result_dialog_key_(key, key_name)) {
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

void DeviceInfoPage::scroll_bounce_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<DeviceInfoPage*>(lv_timer_get_user_data(timer));
  if (!page || !page->viewport_) {
    return;
  }

  lv_obj_scroll_to_y(page->viewport_, page->scroll_bounce_target_y_, LV_ANIM_ON);
}

void DeviceInfoPage::refresh_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<DeviceInfoPage*>(lv_timer_get_user_data(timer));
  if (!page) {
    return;
  }

  page->refresh_();
}

}  // namespace screen
