/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "perf_test_model.h"

namespace model {
namespace {

const std::array<PerfMenuItem, PerfTestModel::K_ITEM_COUNT>& perf_menu_items() {
  static const std::array<PerfMenuItem, PerfTestModel::K_ITEM_COUNT> ITEMS = {{
      {"CPU Benchmark", PerfSubPage::CPU},
      {"Memory Stress", PerfSubPage::MEM},
      {"SD Card Test", PerfSubPage::SD},
  }};
  return ITEMS;
}

}  // namespace

const std::array<PerfMenuItem, PerfTestModel::K_ITEM_COUNT>& PerfTestModel::items() const {
  return perf_menu_items();
}

std::size_t PerfTestModel::selected_index() const { return selected_index_; }

PerfSubPage PerfTestModel::active_page() const { return active_page_; }

void PerfTestModel::select_previous() {
  if (selected_index_ > 0) {
    --selected_index_;
  }
}

void PerfTestModel::select_next() {
  if (selected_index_ + 1 < K_ITEM_COUNT) {
    ++selected_index_;
  }
}

void PerfTestModel::set_selected_index(std::size_t index) {
  if (index < K_ITEM_COUNT) {
    selected_index_ = index;
  }
}

void PerfTestModel::activate_selected() {
  const auto& item = items()[selected_index_];
  active_page_     = item.target_page;
}

void PerfTestModel::show_subpage(PerfSubPage page) {
  if (page == PerfSubPage::MENU) {
    show_menu();
    return;
  }

  for (std::size_t i = 0; i < items().size(); ++i) {
    if (items()[i].target_page == page) {
      selected_index_ = i;
      active_page_    = page;
      return;
    }
  }
}

void PerfTestModel::show_menu() { active_page_ = PerfSubPage::MENU; }

}  // namespace model
