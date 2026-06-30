/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base_screen.h"
#include "icon_list.h"

namespace screen {

class TestResultPage : public BaseScreen {
 public:
  TestResultPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~TestResultPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  void move_selection_(int32_t direction);
  static void key_listener(uint32_t key, const char* key_name, void* user_data);

  std::vector<std::string> result_titles_{};
  std::unique_ptr<view::widgets::IconList> result_list_{};
  std::size_t selected_index_{0};
  std::size_t item_count_{0};
};

}  // namespace screen
