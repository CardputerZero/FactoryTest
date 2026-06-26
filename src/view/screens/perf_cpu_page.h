/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "lvgl.h"
#include "perf_command_view.h"

namespace screen {

class PerfCpuView {
 public:
  PerfCpuView()  = default;
  ~PerfCpuView() = default;
  void build(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);

 private:
  std::unique_ptr<PerfCommandView> command_view_{};
};

}  // namespace screen
