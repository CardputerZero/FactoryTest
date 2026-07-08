/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>

#include "app_model.h"

namespace model {

enum class StartMenuCategory {
  AUTO   = 0,
  PERIPH = 1,
  COMMS  = 2,
};

struct StartMenuItem {
  const char* title;
  const char* icon;
  StartMenuCategory category;
  AppPage target_page;
  bool starts_sequence;
};

class StartMenuModel {
 public:
  static constexpr std::size_t K_ITEM_COUNT     = 24;
  static constexpr std::size_t K_CATEGORY_COUNT = 3;

  const std::array<StartMenuItem, K_ITEM_COUNT>& items() const;
  StartMenuCategory selected_category() const;
  std::size_t selected_category_index() const;
  std::size_t selected_index() const;
  const StartMenuItem& selected_item() const;
  std::size_t item_count_for_category(StartMenuCategory category) const;
  const StartMenuItem* item_for_category(StartMenuCategory category, std::size_t index) const;
  void select_previous();
  void select_next();
  void set_selected_index(std::size_t index);
  void select_previous_category();
  void select_next_category();
  void set_selected_category(StartMenuCategory category);
  bool drawer_hidden() const;
  void set_drawer_hidden(bool hidden);

 private:
  std::size_t absolute_index_for_category_(StartMenuCategory category, std::size_t index) const;
  std::size_t selected_index_for_category_(StartMenuCategory category) const;

  StartMenuCategory selected_category_{StartMenuCategory::AUTO};
  std::array<std::size_t, K_CATEGORY_COUNT> selected_indices_{};
  bool drawer_hidden_{false};
};

}  // namespace model
