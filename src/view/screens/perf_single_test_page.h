/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <memory>

#include "app_model.h"
#include "base_screen.h"
#include "perf_cpu_page.h"
#include "perf_mem_page.h"
#include "perf_sd_page.h"

namespace screen {

class PerfSingleTestPage : public BaseScreen {
 public:
  PerfSingleTestPage(viewmodel::AppViewModel& app_view_model,
                     app::AssetManager& assets,
                     model::AppPage page);
  ~PerfSingleTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  void update_nav_actions_();

  model::AppPage page_;
  std::unique_ptr<PerfCpuView> cpu_view_{};
  std::unique_ptr<PerfMemView> mem_view_{};
  std::unique_ptr<PerfSdView> sd_view_{};
};

}  // namespace screen
