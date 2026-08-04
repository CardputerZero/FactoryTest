/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base_screen.h"
#include "icon_list.h"
#include "popup.h"

namespace screen {

class TestResultPage : public BaseScreen {
 public:
  TestResultPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~TestResultPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  void move_selection_(int32_t direction);
  void start_upload_();
  void refresh_upload_();
  void show_upload_popup_(const std::string& message,
                          view::widgets::PopupTone tone,
                          uint32_t duration_ms);
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void upload_timer_cb(lv_timer_t* timer);

  std::vector<std::string> result_titles_{};
  std::unique_ptr<view::widgets::IconList> result_list_{};
  std::unique_ptr<view::widgets::Popup> upload_popup_{};
  lv_timer_t* upload_timer_{nullptr};
  std::uint64_t last_upload_revision_{0};
  std::size_t selected_index_{0};
  std::size_t item_count_{0};
  bool upload_requested_{false};
};

}  // namespace screen
