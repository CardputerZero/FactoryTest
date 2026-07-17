/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_page.h"

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "io_page_common.h"
#include "ui_const.h"

namespace screen {
namespace {

std::string bssid_suffix(std::string_view bssid) {
  std::string hex_digits;
  hex_digits.reserve(12);
  for (const unsigned char ch : bssid) {
    if (std::isxdigit(ch)) {
      hex_digits.push_back(static_cast<char>(std::toupper(ch)));
    }
  }
  if (hex_digits.size() != 12) {
    return {};
  }

  return hex_digits.substr(8, 2) + ":" + hex_digits.substr(10, 2);
}

std::string wifi_display_name(const model::ScanItem& item) {
  std::string name = item.name.empty() ? std::string{"<hidden>"} : item.name;
  const auto suffix = bssid_suffix(item.bssid);
  if (!suffix.empty()) {
    name += " [" + suffix + "]";
  }
  return name;
}

}  // namespace

const char* wifi_icon_for_strength(int32_t strength_percent) {
  if (strength_percent < 0) {
    return view::ICON_WIFI_NONE;
  }
  if (strength_percent < 35) {
    return view::ICON_WIFI_LOW;
  }
  if (strength_percent < 70) {
    return view::ICON_WIFI_MEDIUM;
  }
  return view::ICON_WIFI_HIGH;
}

std::vector<ScanPanelRow> wifi_panel_rows(const std::vector<model::ScanItem>& items,
                                          const std::string& error_message) {
  std::vector<ScanPanelRow> rows;
  rows.reserve(items.size() + 1);
  if (!error_message.empty()) {
    rows.push_back({view::ICON_INFO, error_message});
  }
  for (const auto& item : items) {
    rows.push_back({wifi_icon_for_strength(item.strength_percent), wifi_display_name(item)});
  }
  if (items.empty() && error_message.empty()) {
    rows.push_back({view::ICON_INFO, "No Wi-Fi access points found"});
  }
  return rows;
}

WifiConnectivityView::WifiConnectivityView(viewmodel::WifiConnectivityViewModel& view_model)
    : view_model_(view_model) {}

WifiConnectivityView::~WifiConnectivityView() { delete_timer(refresh_timer_); }

void WifiConnectivityView::build(lv_obj_t* parent,
                                 viewmodel::AppViewModel& app_view_model,
                                 app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  panel_          = build_scan_panel(parent, app_view_model);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_POLL_MS, this);
}

void WifiConnectivityView::refresh_() {
  const bool changed = view_model_.refresh();
  if ((changed || !panel_initialized_) && app_view_model_ && assets_) {
    const auto rows = wifi_panel_rows(view_model_.scan_items(), view_model_.error_message());
    rebuild_scan_panel(panel_, *app_view_model_, *assets_, rows);
    panel_initialized_ = true;
  }
}

void WifiConnectivityView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<WifiConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->refresh_();
  }
}

}  // namespace screen
