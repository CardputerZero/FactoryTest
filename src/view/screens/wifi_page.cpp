/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_page.h"

#include "io_page_common.h"

#include <string>
#include <vector>

#include "ui_const.h"

namespace screen {

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

std::vector<ScanPanelRow> wifi_panel_rows(const std::vector<model::ConnectivityScanInfo>& items,
                                          const std::string& error_message) {
  std::vector<ScanPanelRow> rows;
  rows.reserve(items.size() + 1);
  if (!error_message.empty()) {
    rows.push_back({view::ICON_INFO, error_message});
  }
  for (const auto& item : items) {
    rows.push_back({wifi_icon_for_strength(item.strength_percent),
                    item.name.empty() ? std::string{"<hidden>"} : item.name});
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
