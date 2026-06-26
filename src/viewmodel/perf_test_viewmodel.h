/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>

#include "lvgl.h"
#include "perf_test_model.h"
#include "subjects.h"

namespace viewmodel {

class PerfTestViewModel {
 public:
  PerfTestViewModel();
  ~PerfTestViewModel();

  PerfTestViewModel(const PerfTestViewModel&)            = delete;
  PerfTestViewModel& operator=(const PerfTestViewModel&) = delete;

  lv_subject_t* selected_index_subject();
  lv_subject_t* active_page_subject();

  const std::array<model::PerfMenuItem, model::PerfTestModel::K_ITEM_COUNT>& items() const;
  std::size_t selected_index() const;
  model::PerfSubPage active_page() const;
  bool is_menu_active() const;
  bool is_direct_subpage_active() const;

  void select_previous();
  void select_next();
  void set_selected_index(std::size_t index);
  void activate_selected();
  void show_subpage(model::PerfSubPage page, bool direct = false);
  void show_menu();
  void clear_direct_subpage();
  bool request_back();

 protected:
  void publish_all_();

 private:
  model::PerfTestModel model_{};
  reactive::IntSubject selected_index_subject_;
  reactive::IntSubject active_page_subject_;
  bool direct_subpage_active_{false};
};

}  // namespace viewmodel
