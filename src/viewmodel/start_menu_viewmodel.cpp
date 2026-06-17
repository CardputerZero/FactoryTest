/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "start_menu_viewmodel.h"

namespace viewmodel {

StartMenuViewModel::StartMenuViewModel()
    : selected_index_subject_(static_cast<int32_t>(model_.selected_index())) {}

StartMenuViewModel::~StartMenuViewModel() = default;

lv_subject_t* StartMenuViewModel::selected_index_subject() {
  return selected_index_subject_.native();
}

const std::array<model::StartMenuItem, model::StartMenuModel::K_ITEM_COUNT>&
StartMenuViewModel::items() const {
  return model_.items();
}

std::size_t StartMenuViewModel::selected_index() const { return model_.selected_index(); }

const model::StartMenuItem& StartMenuViewModel::selected_item() const {
  return model_.items()[model_.selected_index()];
}

void StartMenuViewModel::select_previous() {
  model_.select_previous();
  publish_all_();
}

void StartMenuViewModel::select_next() {
  model_.select_next();
  publish_all_();
}

void StartMenuViewModel::set_selected_index(std::size_t index) {
  model_.set_selected_index(index);
  publish_all_();
}

void StartMenuViewModel::publish_all_() {
  selected_index_subject_.set(static_cast<int32_t>(model_.selected_index()));
}

}  // namespace viewmodel
