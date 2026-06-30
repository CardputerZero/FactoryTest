/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "app_viewmodel.h"
#include "base_widget.h"

namespace view::widgets {

class IconList : public BaseWidgets {
 public:
  enum class Status {
    NONE,
    PASS,
    WARN,
    FAIL,
  };

  struct Item {
    const char* icon;
    const char* title;
    bool hidden{false};
    Status status{Status::NONE};
  };

  using ItemClickedCallback = void (*)(std::size_t index, void* user_data);

  IconList(lv_obj_t* parent,
           viewmodel::AppViewModel& view_model,
           const std::vector<Item>& items,
           const lv_font_t* text_font,
           const lv_font_t* icon_font,
           int32_t width,
           int32_t height,
           ItemClickedCallback clicked_callback,
           void* user_data = nullptr);

  void build() override;
  void set_selected_index(std::size_t index);
  void set_focused(bool focused);
  void set_width(int32_t width);
  void refresh_theme();
  int32_t width() const;
  bool is_focused() const;

 protected:
  void apply_theme(bool dark_mode) override;

 private:
  struct Row {
    lv_obj_t* button{nullptr};
    lv_obj_t* icon_label{nullptr};
    lv_obj_t* text_label{nullptr};
    lv_obj_t* status_label{nullptr};
    std::size_t index{0};
  };

  static void item_clicked_cb(lv_event_t* event);
  bool is_item_visible_(std::size_t index) const;
  std::size_t nearest_visible_index_(std::size_t index) const;
  Row* row_for_index_(std::size_t index);
  void apply_row_style_(Row& row, bool dark_mode);
  void scroll_selected_to_view_();

  viewmodel::AppViewModel& view_model_;
  std::vector<Item> items_{};
  std::vector<Row> rows_{};
  const lv_font_t* text_font_{nullptr};
  const lv_font_t* icon_font_{nullptr};
  int32_t width_{0};
  int32_t height_{0};
  ItemClickedCallback clicked_callback_{nullptr};
  void* user_data_{nullptr};
  std::size_t selected_index_{0};
  bool focused_{true};
};

}  // namespace view::widgets
