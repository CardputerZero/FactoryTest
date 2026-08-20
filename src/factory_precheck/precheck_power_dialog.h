/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <vector>

#include "lvgl.h"

namespace precheck {

enum class PowerAction {
  POWER_OFF,
  REBOOT,
  CANCEL,
};

class PowerDialog {
 public:
  using ActionCallback = void (*)(PowerAction action, void* user_data);

  PowerDialog(lv_obj_t* parent,
              const lv_font_t* text_font,
              const lv_font_t* icon_font,
              ActionCallback callback,
              void* user_data);
  ~PowerDialog();

  PowerDialog(const PowerDialog&) = delete;
  PowerDialog& operator=(const PowerDialog&) = delete;

  void open();
  void close();
  bool visible() const { return visible_; }
  bool handle_key(uint32_t key);

 private:
  struct Row {
    lv_obj_t* button{nullptr};
    lv_obj_t* icon{nullptr};
    lv_obj_t* label{nullptr};
    PowerAction action{PowerAction::CANCEL};
  };

  static void row_clicked_cb(lv_event_t* event);
  static void backdrop_clicked_cb(lv_event_t* event);
  void build_();
  void activate_selected_();
  void activate_(PowerAction action);
  void update_selection_();

  lv_obj_t* parent_{nullptr};
  const lv_font_t* text_font_{nullptr};
  const lv_font_t* icon_font_{nullptr};
  ActionCallback callback_{nullptr};
  void* user_data_{nullptr};
  lv_obj_t* backdrop_{nullptr};
  lv_obj_t* panel_{nullptr};
  lv_obj_t* list_{nullptr};
  std::vector<Row> rows_;
  std::size_t selected_index_{2};
  bool visible_{false};
};

}  // namespace precheck
