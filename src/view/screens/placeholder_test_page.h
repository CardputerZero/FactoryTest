/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

class PlaceholderTestPage : public BaseScreen {
 public:
  PlaceholderTestPage(viewmodel::AppViewModel& app_view_model,
                      app::AssetManager& assets,
                      const char* title,
                      const char* message);
  ~PlaceholderTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  const char* title_{nullptr};
  const char* message_{nullptr};
};

}  // namespace screen
