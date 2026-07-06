/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "base_widget.h"

namespace view::widgets {

class TitleBar : public BaseWidgets {
 public:
  TitleBar(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~TitleBar() override;

  void build() override;
  void set_title(const char* text);
  void set_title_alignment(lv_align_t align, int32_t x_offset = 8, int32_t y_offset = 0);
  void set_center_text(const char* text);

 private:
  void apply_title_alignment_();
  void apply_title_font_();
  static void title_layout_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void language_observer(lv_observer_t* observer, lv_subject_t* subject);

  viewmodel::AppViewModel& app_view_model_;
  app::AssetManager& assets_;
  lv_obj_t* title_label_{nullptr};
  lv_obj_t* center_label_{nullptr};
  lv_align_t title_align_{LV_ALIGN_LEFT_MID};
  int32_t title_x_offset_{8};
  int32_t title_y_offset_{0};
  lv_observer_t* title_align_observer_{nullptr};
  lv_observer_t* title_x_observer_{nullptr};
  lv_observer_t* title_y_observer_{nullptr};
  lv_observer_t* title_text_observer_{nullptr};
  lv_observer_t* language_observer_{nullptr};
};

}  // namespace view::widgets
