/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "usb_page.h"

#include <string>
#include <vector>

#include "io_page_common.h"
#include "ui_const.h"

namespace screen {

std::vector<CardSpec> usb_card_specs(const std::vector<model::ScanItem>& items,
                                     const std::string& error_message) {
  std::vector<CardSpec> specs;
  specs.reserve(items.size() + 1);
  if (!error_message.empty()) {
    specs.push_back({view::ICON_INFO, "Status", error_message});
  }
  for (const auto& item : items) {
    const bool name_only = item.detail.empty() && !item.name.empty() && item.name != "USB Device";
    const auto title =
        name_only ? std::string{} : (item.detail.empty() ? std::string{"USB"} : item.detail);
    const auto value = item.name.empty() ? std::string{"USB Device"} : item.name;
    specs.push_back({view::ICON_USB, title, value});
  }
  if (items.empty() && error_message.empty()) {
    specs.push_back({view::ICON_INFO, "Status", "No USB devices found"});
  }
  return specs;
}

UsbConnectivityView::UsbConnectivityView(viewmodel::UsbConnectivityViewModel& view_model)
    : view_model_(view_model) {}

UsbConnectivityView::~UsbConnectivityView() { delete_timer(refresh_timer_); }

void UsbConnectivityView::build(lv_obj_t* parent,
                                viewmodel::AppViewModel& app_view_model,
                                app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  list_           = build_card_list(parent, app_view_model);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_POLL_MS, this);
}

void UsbConnectivityView::refresh_() {
  const bool changed = view_model_.refresh();
  if (cards_need_refresh(cards_, changed) && app_view_model_ && assets_) {
    const auto specs = usb_card_specs(view_model_.devices(), view_model_.error_message());
    rebuild_cards(list_, cards_, *app_view_model_, *assets_, specs);
  }
}

void UsbConnectivityView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<UsbConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->refresh_();
  }
}

}  // namespace screen
