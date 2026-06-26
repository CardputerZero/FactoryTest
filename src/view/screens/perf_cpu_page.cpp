/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "perf_cpu_page.h"

#include "cpu_benchmark_service.h"

namespace screen {

void PerfCpuView::build(lv_obj_t* parent,
                        viewmodel::AppViewModel& app_view_model,
                        app::AssetManager& assets) {
  command_view_ = std::make_unique<PerfCommandView>("CPU Benchmark",
                                                    platform::perf::make_cpu_sysbench_command(),
                                                    platform::perf::run_cpu_sysbench);
  command_view_->build(parent, app_view_model, assets);
}

}  // namespace screen
