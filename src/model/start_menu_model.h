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

struct StartMenuItem {
  const char* title;
  AppPage target_page;
  bool starts_sequence;
};

class StartMenuModel {
 public:
  static constexpr std::size_t K_ITEM_COUNT = 9;

  const std::array<StartMenuItem, K_ITEM_COUNT>& items() const;
  std::size_t selected_index() const;
  void select_previous();
  void select_next();
  void set_selected_index(std::size_t index);

 private:
  std::size_t selected_index_{0};
};

}  // namespace model
