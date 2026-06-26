/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "perf_mem_page.h"

#include "cpu_benchmark_service.h"

namespace screen {

void PerfMemView::build(lv_obj_t* parent,
                        viewmodel::AppViewModel& app_view_model,
                        app::AssetManager& assets) {
  command_view_ = std::make_unique<PerfCommandView>("Memory Stress",
                                                    platform::perf::make_memory_stress_ng_command(),
                                                    platform::perf::run_memory_stress_ng);
  command_view_->build(parent, app_view_model, assets);
}

}  // namespace screen
