/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "base_screen.h"
#include "icon_card.h"
#include "imu_service.h"

namespace screen {

class ImuTestPage : public BaseScreen {
 public:
  ImuTestPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~ImuTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void poll_timer_cb(lv_timer_t* timer);
  static void scroll_bounce_timer_cb(lv_timer_t* timer);
  void update_readings_();
  void set_value_(std::size_t index, double value, const char* unit);
  void set_missing_(std::size_t index, const char* message);
  void scroll_(int32_t direction);

  platform::imu::ImuDevice device_{};
  std::string status_message_{};
  bool has_imu_{false};
  lv_obj_t* status_label_{nullptr};
  lv_obj_t* viewport_{nullptr};
  lv_obj_t* grid_{nullptr};
  std::array<std::unique_ptr<view::widgets::IconCard>, 9> cards_{};
  lv_timer_t* poll_timer_{nullptr};
  lv_timer_t* scroll_bounce_timer_{nullptr};
  int32_t scroll_bounce_target_y_{0};
};

}  // namespace screen
