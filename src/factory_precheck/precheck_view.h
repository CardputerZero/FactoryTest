/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "asset_manager.h"
#include "precheck_power_dialog.h"
#include "precheck_viewmodel.h"
#include "lvgl.h"

namespace precheck {

class PrecheckView {
 public:
  PrecheckView(PrecheckViewModel& view_model, app::AssetManager& assets);
  ~PrecheckView();

  PrecheckView(const PrecheckView&) = delete;
  PrecheckView& operator=(const PrecheckView&) = delete;

  void build(lv_display_t* display);
  bool running() const { return running_; }
  static void key_listener(uint32_t key, bool pressed, void* user_data);

 private:
  void refresh_();
  void scroll_list_(int32_t direction);
  static void refresh_timer_cb(lv_timer_t* timer);
  static void power_action_cb(PowerAction action, void* user_data);
  void open_power_dialog_();
  void handle_power_action_(PowerAction action);

  PrecheckViewModel& view_model_;
  app::AssetManager& assets_;
  lv_obj_t* root_{nullptr};
  lv_obj_t* title_{nullptr};
  lv_obj_t* device_{nullptr};
  lv_obj_t* progress_{nullptr};
  lv_obj_t* status_{nullptr};
  lv_obj_t* summary_{nullptr};
  lv_obj_t* list_{nullptr};
  lv_obj_t* detail_panel_{nullptr};
  lv_obj_t* detail_title_{nullptr};
  lv_obj_t* detail_text_{nullptr};
  // Parent rows are kept separately from expanded child rows so top-level
  // item indices remain stable when CAP or IR sub-items are displayed.
  std::vector<lv_obj_t*> item_rows_;
  std::vector<lv_obj_t*> cap_sub_rows_;
  std::vector<lv_obj_t*> ir_sub_rows_;
  std::size_t displayed_current_index_{static_cast<std::size_t>(-1)};
  lv_timer_t* refresh_timer_{nullptr};
  lv_font_t* title_font_{nullptr};
  lv_font_t* text_font_{nullptr};
  lv_font_t* small_font_{nullptr};
  lv_font_t* icon_font_{nullptr};
  std::unique_ptr<PowerDialog> power_dialog_;
  bool running_{true};
};

}  // namespace precheck
