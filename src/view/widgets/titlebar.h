/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "app_viewmodel.h"
#include "base_widget.h"

namespace view::widgets {

class TitleBar : public BaseWidgets {
 public:
  TitleBar(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model);

  void build() override;
  void set_title_alignment(lv_align_t align, int32_t x_offset = 0, int32_t y_offset = 0);
  void set_center_text(const char* text);

 private:
  viewmodel::AppViewModel& app_view_model_;
  lv_obj_t* title_label_{nullptr};
  lv_obj_t* center_label_{nullptr};
  lv_align_t title_align_{LV_ALIGN_CENTER};
  int32_t title_x_offset_{0};
  int32_t title_y_offset_{0};
};

}  // namespace view::widgets
