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
  const std::array<model::StartMenuItem, model::StartMenuModel::K_ITEM_COUNT>& items() const;
  std::size_t                 selected_index() const;
  const model::StartMenuItem& selected_item() const;
  void                        select_previous();
  void                        select_next();
  void                        set_selected_index(std::size_t index);

 protected:
  void publish_all_();

 private:
  model::StartMenuModel model_{};
  reactive::IntSubject  selected_index_subject_;
};

}  // namespace viewmodel
