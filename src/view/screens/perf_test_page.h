/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <memory>

#include "base_screen.h"
#include "icon_list.h"
#include "perf_cpu_page.h"
#include "perf_mem_page.h"
#include "perf_sd_page.h"
#include "perf_test_viewmodel.h"

namespace screen {

class PerfTestPage : public BaseScreen {
 public:
  PerfTestPage(viewmodel::AppViewModel& app_view_model,
               viewmodel::PerfTestViewModel& perf_view_model,
               app::AssetManager& assets);
  ~PerfTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  static bool back_request_handler(void* user_data);
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void menu_item_clicked(std::size_t index, void* user_data);
  static void selected_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void active_page_observer(lv_observer_t* observer, lv_subject_t* subject);

  std::size_t viewport_index_(model::PerfSubPage page) const;
  void show_page_(model::PerfSubPage page);
  void update_selection_(std::size_t index);
  lv_obj_t* viewport_for_page_(model::PerfSubPage page) const;
  void reset_subpage_views_(model::PerfSubPage keep_page);
  void ensure_subpage_view_(model::PerfSubPage page);
  void update_title_(model::PerfSubPage page);
  void update_nav_actions_();

  viewmodel::PerfTestViewModel& perf_view_model_;
  lv_obj_t* plane_{nullptr};
  std::unique_ptr<view::widgets::IconList> menu_list_{};
  std::unique_ptr<PerfCpuView> cpu_view_{};
  std::unique_ptr<PerfMemView> mem_view_{};
  std::unique_ptr<PerfSdView> sd_view_{};
  lv_observer_t* selected_observer_handle_{nullptr};
  lv_observer_t* active_page_observer_handle_{nullptr};
};

}  // namespace screen
