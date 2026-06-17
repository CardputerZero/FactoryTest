/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl.h"

namespace view {

constexpr int K_SCREEN_WIDTH     = 320;
constexpr int K_SCREEN_HEIGHT    = 170;
constexpr int K_TITLE_BAR_HEIGHT = 30;
constexpr int K_NAV_BAR_HEIGHT   = 30;

struct ThemePalette {
  lv_color_t background;
  lv_color_t surface;
  lv_color_t bar;
  lv_color_t button;
  lv_color_t border;
  lv_color_t text;
  lv_color_t text_muted;
  lv_color_t text_disabled;
  lv_color_t primary;
  lv_color_t info;
  lv_color_t success;
  lv_color_t warning;
  lv_color_t error;
  lv_color_t primary_hover;
  lv_color_t primary_pressed;
  lv_color_t info_hover;
  lv_color_t info_pressed;
  lv_color_t success_hover;
  lv_color_t success_pressed;
  lv_color_t warning_hover;
  lv_color_t warning_pressed;
  lv_color_t error_hover;
  lv_color_t error_pressed;
};

ThemePalette palette(bool dark_mode);
void apply_lvgl_theme(lv_display_t* display, bool dark_mode);
void apply_slider_theme(lv_obj_t* slider, bool dark_mode);
void apply_chart_theme(lv_obj_t* chart, lv_chart_series_t* series, bool dark_mode);
void set_style_color(lv_obj_t* obj, lv_style_prop_t prop, lv_color_t color);
lv_color_t get_style_color(lv_obj_t* obj, lv_style_prop_t prop);
void animate_style_color(lv_obj_t* obj,
                         lv_style_prop_t prop,
                         lv_color_t target,
                         uint32_t duration_ms = 220);

}  // namespace view
