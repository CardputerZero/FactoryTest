/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <vector>

#include "base_screen.h"
#include "icon_card.h"

namespace screen {

class DeviceInfoPage : public BaseScreen {
 public:
  DeviceInfoPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~DeviceInfoPage() override;

 protected:
  void build_content(lv_obj_t* content) override;
  void refresh_();
  void scroll_(int32_t direction);
  static void refresh_timer_cb(lv_timer_t* timer);
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void scroll_bounce_timer_cb(lv_timer_t* timer);

 private:
  lv_obj_t* viewport_{nullptr};
  lv_obj_t* grid_{nullptr};
  std::vector<std::unique_ptr<view::widgets::IconCard>> cards_{};
  lv_timer_t* refresh_timer_{nullptr};
  lv_timer_t* scroll_bounce_timer_{nullptr};
  int32_t scroll_bounce_target_y_{0};
};

}  // namespace screen
