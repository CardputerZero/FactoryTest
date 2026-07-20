/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstdint>

#include "base_screen.h"
#include "cap_fixture_test_model.h"

namespace screen {

class CapFixtureTestPage : public BaseScreen {
 public:
  CapFixtureTestPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~CapFixtureTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  static void refresh_timer_cb(lv_timer_t* timer);
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  void refresh_();
  void scroll_(int32_t direction);
  void cancel_and_leave_();
  void submit_automated_result_(const model::CapFixtureSnapshot& snapshot);

  model::CapFixtureTestModel model_{};
  lv_obj_t* viewport_{nullptr};
  lv_obj_t* grid_{nullptr};
  lv_obj_t* headline_label_{nullptr};
  lv_obj_t* error_label_{nullptr};
  std::array<lv_obj_t*, 6> row_objects_{};
  std::array<lv_obj_t*, 6> status_labels_{};
  std::array<lv_obj_t*, 6> detail_labels_{};
  lv_timer_t* refresh_timer_{nullptr};
  unsigned int displayed_revision_{static_cast<unsigned int>(-1)};
  bool completion_sent_{false};
};

}  // namespace screen
