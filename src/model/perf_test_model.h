/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>

namespace model {

enum class PerfSubPage {
  MENU = 0,
  CPU  = 1,
  MEM  = 2,
  SD   = 3,
};

struct PerfMenuItem {
  const char* title;
  PerfSubPage target_page;
};

class PerfTestModel {
 public:
  static constexpr std::size_t K_ITEM_COUNT = 3;

  const std::array<PerfMenuItem, K_ITEM_COUNT>& items() const;
  std::size_t selected_index() const;
  PerfSubPage active_page() const;
  void select_previous();
  void select_next();
  void set_selected_index(std::size_t index);
  void activate_selected();
  void show_subpage(PerfSubPage page);
  void show_menu();

 private:
  std::size_t selected_index_{0};
  PerfSubPage active_page_{PerfSubPage::MENU};
};

}  // namespace model
