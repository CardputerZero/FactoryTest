/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>

#include "base_screen.h"
#include "ir_fixture_test_model.h"

namespace screen {

class IrFixtureTestPage : public BaseScreen {
 public:
  IrFixtureTestPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~IrFixtureTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  static void refresh_timer_cb(lv_timer_t* timer);
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  void refresh_();
  void start_();
  void report_result_();
  void cancel_and_leave_();

  model::IrFixtureTestModel model_{};
  lv_obj_t* status_label_{nullptr};
  std::array<lv_obj_t*, 2> status_values_{};
  std::array<lv_obj_t*, 2> detail_values_{};
  lv_timer_t* refresh_timer_{nullptr};
  unsigned int displayed_revision_{static_cast<unsigned int>(-1)};
};

}  // namespace screen
