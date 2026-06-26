/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "start_menu_viewmodel.h"

namespace viewmodel {

StartMenuViewModel::StartMenuViewModel()
    : selected_index_subject_(static_cast<int32_t>(model_.selected_index())),
      selected_category_subject_(static_cast<int32_t>(model_.selected_category_index())) {}

StartMenuViewModel::~StartMenuViewModel() = default;

lv_subject_t* StartMenuViewModel::selected_index_subject() {
  return selected_index_subject_.native();
}

lv_subject_t* StartMenuViewModel::selected_category_subject() {
  return selected_category_subject_.native();
}

const std::array<model::StartMenuItem, model::StartMenuModel::K_ITEM_COUNT>&
StartMenuViewModel::items() const {
  return model_.items();
}

model::StartMenuCategory StartMenuViewModel::selected_category() const {
  return model_.selected_category();
}

std::size_t StartMenuViewModel::selected_category_index() const {
  return model_.selected_category_index();
}

std::size_t StartMenuViewModel::selected_index() const { return model_.selected_index(); }

const model::StartMenuItem& StartMenuViewModel::selected_item() const {
  return model_.selected_item();
}

std::size_t StartMenuViewModel::item_count_for_category(model::StartMenuCategory category) const {
  return model_.item_count_for_category(category);
}

const model::StartMenuItem* StartMenuViewModel::item_for_category(model::StartMenuCategory category,
                                                                  std::size_t index) const {
  return model_.item_for_category(category, index);
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

void StartMenuViewModel::select_previous_category() {
  model_.select_previous_category();
  publish_all_();
}

void StartMenuViewModel::select_next_category() {
  model_.select_next_category();
  publish_all_();
}

void StartMenuViewModel::set_selected_category(model::StartMenuCategory category) {
  model_.set_selected_category(category);
  publish_all_();
}

bool StartMenuViewModel::is_drawer_hidden() const { return model_.drawer_hidden(); }

void StartMenuViewModel::set_drawer_hidden(bool hidden) { model_.set_drawer_hidden(hidden); }

void StartMenuViewModel::publish_all_() {
  selected_index_subject_.set(static_cast<int32_t>(model_.selected_index()));
  selected_category_subject_.set(static_cast<int32_t>(model_.selected_category_index()));
}

}  // namespace viewmodel
