/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_bt_page.h"

#include "connectivity_subpage_common.h"

#include <string>
#include <vector>

#include "ui_const.h"

namespace screen {

std::vector<ScanPanelRow> bluetooth_panel_rows(
    const std::vector<model::ConnectivityScanInfo>& items,
    const std::string& error_message) {
  std::vector<ScanPanelRow> rows;
  rows.reserve(items.size() + 1);
  if (!error_message.empty()) {
    rows.push_back({view::ICON_INFO, error_message});
  }
  for (const auto& item : items) {
    const auto text = !item.name.empty()
                          ? item.name
                          : (!item.detail.empty() ? item.detail : std::string{"Bluetooth Device"});
    rows.push_back({view::ICON_BLUETOOTH, text});
  }
  if (items.empty() && error_message.empty()) {
    rows.push_back({view::ICON_INFO, "No Bluetooth devices found"});
  }
  return rows;
}

BluetoothConnectivityView::BluetoothConnectivityView(
    viewmodel::BluetoothConnectivityViewModel& view_model)
    : view_model_(view_model) {}

BluetoothConnectivityView::~BluetoothConnectivityView() { delete_timer(refresh_timer_); }

void BluetoothConnectivityView::build(lv_obj_t* parent,
                                      viewmodel::AppViewModel& app_view_model,
                                      app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  panel_          = build_scan_panel(parent, app_view_model);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_POLL_MS, this);
}

void BluetoothConnectivityView::refresh_() {
  const bool changed = view_model_.refresh();
  if ((changed || !panel_initialized_) && app_view_model_ && assets_) {
    const auto rows = bluetooth_panel_rows(view_model_.scan_items(), view_model_.error_message());
    rebuild_scan_panel(panel_, *app_view_model_, *assets_, rows);
    panel_initialized_ = true;
  }
}

void BluetoothConnectivityView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<BluetoothConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->refresh_();
  }
}

}  // namespace screen
