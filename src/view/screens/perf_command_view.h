/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "cpu_benchmark_service.h"
#include "icon_list.h"
#include "lvgl.h"

namespace screen {

class PerfCommandView {
 public:
  using Runner = std::function<platform::perf::TestResult(platform::perf::ProgressCallback)>;

  PerfCommandView(std::string title, platform::perf::TestCommand command, Runner runner);
  ~PerfCommandView();

  PerfCommandView(const PerfCommandView&)            = delete;
  PerfCommandView& operator=(const PerfCommandView&) = delete;

  void build(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);

 private:
  struct JobState {
    std::atomic<bool> done{false};
    std::atomic<int> percent{0};
    std::atomic<bool> has_real_progress{false};
    std::atomic<uint32_t> line_revision{0};
    std::mutex mutex{};
    std::deque<std::string> lines{};
    platform::perf::TestResult result{};
  };

  void start_();
  void update_();
  void update_progress_(int percent);
  int displayed_progress_(int percent);
  void update_stdout_buffer_();
  void show_report_(const platform::perf::TestResult& result);
  std::vector<std::string> report_lines_(const platform::perf::TestResult& result) const;
  std::string stdout_text_() const;

  static void timer_cb(lv_timer_t* timer);
  static void report_item_clicked(std::size_t index, void* user_data);

  std::string title_{};
  platform::perf::TestCommand command_{};
  Runner runner_{};
  std::shared_ptr<JobState> state_{};
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* status_label_{nullptr};
  lv_obj_t* command_label_{nullptr};
  lv_obj_t* progress_bar_{nullptr};
  lv_obj_t* stdout_card_{nullptr};
  lv_obj_t* stdout_label_{nullptr};
  lv_obj_t* report_host_{nullptr};
  lv_timer_t* poll_timer_{nullptr};
  std::unique_ptr<view::widgets::IconList> report_list_{};
  std::vector<std::string> report_titles_{};
  std::vector<view::widgets::IconList::Item> report_items_{};
  uint32_t last_line_revision_{0};
  int fake_progress_{0};
  int fake_progress_tick_{0};
  bool report_shown_{false};
};

}  // namespace screen
