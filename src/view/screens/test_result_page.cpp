/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "test_result_page.h"

#include <cctype>
#include <string>
#include <vector>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "process_service.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_LIST_WIDTH  = 300;
constexpr int32_t K_LIST_HEIGHT = 106;

const char* icon_for_test_name(const std::string& name) {
  if (name == "Input Test") {
    return view::ICON_KEYBOARD;
  }
  if (name == "Display Test" || name == "HDMI") {
    return view::ICON_MONITOR;
  }
  if (name == "Audio Test") {
    return view::ICON_MICROPHONE;
  }
  if (name == "Camera Test") {
    return view::ICON_CAMERA;
  }
  if (name == "IR Test") {
    return view::ICON_BROADCAST;
  }
  if (name == "Wi-Fi") {
    return view::ICON_WIFI;
  }
  if (name == "Bluetooth") {
    return view::ICON_BLUETOOTH;
  }
  if (name == "Ethernet") {
    return view::ICON_ETHERNET;
  }
  if (name == "USB") {
    return view::ICON_USB;
  }
  if (name == "I2C") {
    return view::ICON_SCAN;
  }
  if (name == "SPI") {
    return view::ICON_LINK_SIMPLE_HOR;
  }
  if (name == "UART") {
    return view::ICON_BROADCAST;
  }
  if (name == "EXT.IO") {
    return view::ICON_PLUGS_CONNECTED;
  }
  if (name == "Link Test") {
    return view::ICON_GLOBE;
  }
  if (name == "Device Information") {
    return view::ICON_INFO;
  }
  if (name == "Power Information") {
    return view::ICON_LIGHTNING;
  }
  if (name == "IMU Test") {
    return view::ICON_VECTOR_THREE;
  }
  if (name == "CPU Benchmark") {
    return view::ICON_CPU;
  }
  if (name == "Mem Stress Test") {
    return view::ICON_MEMORY;
  }
  if (name == "SD Card Test") {
    return view::ICON_HARDDRIVE;
  }
  return view::ICON_INFO;
}

view::widgets::IconList::Status status_for_result(std::string result) {
  for (char& ch : result) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  if (result == "pass" || result == "passed") {
    return view::widgets::IconList::Status::PASS;
  }
  if (result == "skip" || result == "skipped") {
    return view::widgets::IconList::Status::WARN;
  }
  if (result == "fail" || result == "failed") {
    return view::widgets::IconList::Status::FAIL;
  }
  return view::widgets::IconList::Status::NONE;
}

std::vector<view::widgets::IconList::Item> load_result_items(
    const std::string& path,
    std::vector<std::string>& titles) {
  std::vector<view::widgets::IconList::Item> items;
  titles.clear();
  const auto parsed = platform::process::parse_json_file(path);
  if (!parsed.success() ||
      parsed.value.type != platform::process::OutputValue::Type::Object) {
    titles.push_back("No test results found");
    items.push_back({view::ICON_INFO, titles.back().c_str(), false,
                     view::widgets::IconList::Status::WARN});
    return items;
  }

  const auto results_it = parsed.value.object_values.find("results");
  if (results_it == parsed.value.object_values.end() ||
      results_it->second.type != platform::process::OutputValue::Type::Array) {
    titles.push_back("No test results found");
    items.push_back({view::ICON_INFO, titles.back().c_str(), false,
                     view::widgets::IconList::Status::WARN});
    return items;
  }

  items.reserve(results_it->second.array_values.size());
  titles.reserve(results_it->second.array_values.size());
  for (const auto& entry : results_it->second.array_values) {
    if (entry.type != platform::process::OutputValue::Type::Object) {
      continue;
    }

    const auto name_it   = entry.object_values.find("test_name");
    const auto result_it = entry.object_values.find("test_result");
    const std::string name =
        name_it != entry.object_values.end() &&
                name_it->second.type == platform::process::OutputValue::Type::String
            ? name_it->second.string_value
            : "Unknown Test";
    const std::string result =
        result_it != entry.object_values.end() &&
                result_it->second.type == platform::process::OutputValue::Type::String
            ? result_it->second.string_value
            : "";

    titles.push_back(name);
    items.push_back(
        {icon_for_test_name(titles.back()), titles.back().c_str(), false, status_for_result(result)});
  }

  if (items.empty()) {
    titles.push_back("No test results found");
    items.push_back({view::ICON_INFO, titles.back().c_str(), false,
                     view::widgets::IconList::Status::WARN});
  }
  return items;
}

}  // namespace

TestResultPage::TestResultPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  set_nav_action_('4', view::ICON_ARROW_U_UP_LEFT, [this]() {
    app_view_model_ref_().show_start_page();
  });
  init();
  platform::set_key_listener(key_listener, this);
}

TestResultPage::~TestResultPage() {
  platform::clear_key_listener(key_listener, this);
  result_list_.reset();
}

void TestResultPage::build_content(lv_obj_t* content) {
  auto* viewport = lv_obj_create(content);
  lv_obj_remove_style_all(viewport);
  lv_obj_set_size(viewport, view::K_SCREEN_WIDTH, K_LIST_HEIGHT);
  lv_obj_align(viewport, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(viewport,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);

  auto items  = load_result_items(app_view_model_ref_().test_result_path(), result_titles_);
  item_count_ = items.size();
  auto* text_font = assets_ref_().load_font("inter-medium.ttf", 14);
  auto* icon_font = assets_ref_().load_font("Phosphor-Fill.ttf", 14);
  result_list_ =
      std::make_unique<view::widgets::IconList>(viewport,
                                                app_view_model_ref_(),
                                                items,
                                                text_font ? text_font : &lv_font_montserrat_14,
                                                icon_font ? icon_font : &lv_font_montserrat_14,
                                                K_LIST_WIDTH,
                                                K_LIST_HEIGHT,
                                                nullptr,
                                                nullptr);
  result_list_->build();
  result_list_->set_selected_index(selected_index_);
  result_list_->set_focused(true);
}

void TestResultPage::move_selection_(int32_t direction) {
  if (!result_list_ || item_count_ == 0 || direction == 0) {
    return;
  }

  if (direction < 0) {
    selected_index_ = selected_index_ == 0 ? 0 : selected_index_ - 1;
  } else if (selected_index_ + 1 < item_count_) {
    ++selected_index_;
  }
  result_list_->set_selected_index(selected_index_);
}

void TestResultPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<TestResultPage*>(user_data);
  if (!page) {
    return;
  }

  switch (key) {
    case LV_KEY_UP:
    case 'f':
    case 'F':
      page->move_selection_(-1);
      break;
    case LV_KEY_DOWN:
    case 'x':
    case 'X':
      page->move_selection_(1);
      break;
    default:
      break;
  }
}

}  // namespace screen
