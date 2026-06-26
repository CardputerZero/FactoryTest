/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "perf_sd_page.h"

#include "cpu_benchmark_service.h"

namespace screen {

void PerfSdView::build(lv_obj_t* parent,
                       viewmodel::AppViewModel& app_view_model,
                       app::AssetManager& assets) {
  command_view_ = std::make_unique<PerfCommandView>("SD Card Test",
                                                    platform::perf::make_sd_card_fio_command(),
                                                    platform::perf::run_sd_card_fio);
  command_view_->build(parent, app_view_model, assets);
}

}  // namespace screen
