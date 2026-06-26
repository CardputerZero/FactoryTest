/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "perf_test_viewmodel.h"

namespace viewmodel {
namespace {

int page_to_int(model::PerfSubPage page) { return static_cast<int>(page); }

}  // namespace

PerfTestViewModel::PerfTestViewModel()
    : selected_index_subject_(static_cast<int32_t>(model_.selected_index())),
      active_page_subject_(page_to_int(model_.active_page())) {}

PerfTestViewModel::~PerfTestViewModel() = default;

lv_subject_t* PerfTestViewModel::selected_index_subject() {
  return selected_index_subject_.native();
}

lv_subject_t* PerfTestViewModel::active_page_subject() { return active_page_subject_.native(); }

const std::array<model::PerfMenuItem, model::PerfTestModel::K_ITEM_COUNT>&
PerfTestViewModel::items() const {
  return model_.items();
}

std::size_t PerfTestViewModel::selected_index() const { return model_.selected_index(); }

model::PerfSubPage PerfTestViewModel::active_page() const { return model_.active_page(); }

bool PerfTestViewModel::is_menu_active() const {
  return model_.active_page() == model::PerfSubPage::MENU;
}

bool PerfTestViewModel::is_direct_subpage_active() const { return direct_subpage_active_; }

void PerfTestViewModel::select_previous() {
  if (!is_menu_active()) {
    return;
  }
  model_.select_previous();
  publish_all_();
}

void PerfTestViewModel::select_next() {
  if (!is_menu_active()) {
    return;
  }
  model_.select_next();
  publish_all_();
}

void PerfTestViewModel::set_selected_index(std::size_t index) {
  model_.set_selected_index(index);
  publish_all_();
}

void PerfTestViewModel::activate_selected() {
  if (!is_menu_active()) {
    return;
  }
  direct_subpage_active_ = false;
  model_.activate_selected();
  publish_all_();
}

void PerfTestViewModel::show_subpage(model::PerfSubPage page, bool direct) {
  direct_subpage_active_ = direct && page != model::PerfSubPage::MENU;
  model_.show_subpage(page);
  publish_all_();
}

void PerfTestViewModel::show_menu() {
  direct_subpage_active_ = false;
  model_.show_menu();
  publish_all_();
}

void PerfTestViewModel::clear_direct_subpage() { direct_subpage_active_ = false; }

bool PerfTestViewModel::request_back() {
  if (is_menu_active()) {
    return false;
  }
  if (direct_subpage_active_) {
    direct_subpage_active_ = false;
    return false;
  }
  show_menu();
  return true;
}

void PerfTestViewModel::publish_all_() {
  selected_index_subject_.set(static_cast<int32_t>(model_.selected_index()));
  active_page_subject_.set(page_to_int(model_.active_page()));
}

}  // namespace viewmodel
