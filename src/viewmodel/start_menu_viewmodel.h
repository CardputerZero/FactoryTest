/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>

#include "lvgl.h"
#include "start_menu_model.h"
#include "subjects.h"

namespace viewmodel {

class StartMenuViewModel {
 public:
  StartMenuViewModel();
  ~StartMenuViewModel();

  StartMenuViewModel(const StartMenuViewModel&)            = delete;
  StartMenuViewModel& operator=(const StartMenuViewModel&) = delete;

  lv_subject_t* selected_index_subject();
  lv_subject_t* selected_category_subject();
  const std::array<model::StartMenuItem, model::StartMenuModel::K_ITEM_COUNT>& items() const;
  model::StartMenuCategory selected_category() const;
  std::size_t selected_category_index() const;
  std::size_t selected_index() const;
  const model::StartMenuItem& selected_item() const;
  std::size_t item_count_for_category(model::StartMenuCategory category) const;
  const model::StartMenuItem* item_for_category(model::StartMenuCategory category,
                                                std::size_t index) const;
  void select_previous();
  void select_next();
  void set_selected_index(std::size_t index);
  void select_previous_category();
  void select_next_category();
  void set_selected_category(model::StartMenuCategory category);
  bool is_drawer_hidden() const;
  void set_drawer_hidden(bool hidden);

 protected:
  void publish_all_();

 private:
  model::StartMenuModel model_{};
  reactive::IntSubject selected_index_subject_;
  reactive::IntSubject selected_category_subject_;
};

}  // namespace viewmodel
