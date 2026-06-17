/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "theme.h"

#include "ui_const.h"

namespace view {
namespace {

struct StyleColorAnim {
  lv_style_prop_t prop;
  lv_color_t from;
  lv_color_t to;
};

void style_color_anim_exec(lv_anim_t* anim, int32_t value) {
  auto* obj  = static_cast<lv_obj_t*>(anim->var);
  auto* data = static_cast<StyleColorAnim*>(lv_anim_get_user_data(anim));
  if (!obj || !data || !lv_obj_is_valid(obj)) {
    return;
  }

  set_style_color(obj, data->prop, lv_color_mix(data->to, data->from, static_cast<uint8_t>(value)));
}

void style_color_anim_deleted(lv_anim_t* anim) {
  delete static_cast<StyleColorAnim*>(lv_anim_get_user_data(anim));
}

}  // namespace

ThemePalette palette(bool dark_mode) {
  if (dark_mode) {
    return {
        lv_color_hex(DARK_BODY),
        lv_color_hex(DARK_CARD),
        lv_color_hex(DARK_ACTION),
        lv_color_hex(DARK_BUTTON),
        lv_color_hex(DARK_BORDER),
        lv_color_hex(DARK_TEXT_PRIMARY),
        lv_color_hex(DARK_TEXT_MUTED),
        lv_color_hex(DARK_TEXT_DISABLED),
        lv_color_hex(DARK_PRIMARY),
        lv_color_hex(DARK_INFO),
        lv_color_hex(DARK_SUCCESS),
        lv_color_hex(DARK_WARNING),
        lv_color_hex(DARK_ERROR),
        lv_color_hex(DARK_PRIMARY_HOVER),
        lv_color_hex(DARK_PRIMARY_PRESSED),
        lv_color_hex(DARK_INFO_HOVER),
        lv_color_hex(DARK_INFO_PRESSED),
        lv_color_hex(DARK_SUCCESS_HOVER),
        lv_color_hex(DARK_SUCCESS_PRESSED),
        lv_color_hex(DARK_WARNING_HOVER),
        lv_color_hex(DARK_WARNING_PRESSED),
        lv_color_hex(DARK_ERROR_HOVER),
        lv_color_hex(DARK_ERROR_PRESSED),
    };
  }

  return {
      lv_color_hex(LIGHT_BODY),
      lv_color_hex(LIGHT_CARD),
      lv_color_hex(LIGHT_ACTION),
      lv_color_hex(LIGHT_BUTTON),
      lv_color_hex(LIGHT_BORDER),
      lv_color_hex(LIGHT_TEXT_PRIMARY),
      lv_color_hex(LIGHT_TEXT_MUTED),
      lv_color_hex(LIGHT_TEXT_DISABLED),
      lv_color_hex(LIGHT_PRIMARY),
      lv_color_hex(LIGHT_INFO),
      lv_color_hex(LIGHT_SUCCESS),
      lv_color_hex(LIGHT_WARNING),
      lv_color_hex(LIGHT_ERROR),
      lv_color_hex(LIGHT_PRIMARY_HOVER),
      lv_color_hex(LIGHT_PRIMARY_PRESSED),
      lv_color_hex(LIGHT_INFO_HOVER),
      lv_color_hex(LIGHT_INFO_PRESSED),
      lv_color_hex(LIGHT_SUCCESS_HOVER),
      lv_color_hex(LIGHT_SUCCESS_PRESSED),
      lv_color_hex(LIGHT_WARNING_HOVER),
      lv_color_hex(LIGHT_WARNING_PRESSED),
      lv_color_hex(LIGHT_ERROR_HOVER),
      lv_color_hex(LIGHT_ERROR_PRESSED),
  };
}

void apply_lvgl_theme(lv_display_t* display, bool dark_mode) {
  if (!display) {
    return;
  }

  const auto colors = palette(dark_mode);
  lv_theme_t* theme =
      lv_theme_default_init(display, colors.primary, colors.info, dark_mode, LV_FONT_DEFAULT);
  lv_display_set_theme(display, theme);
}

void apply_slider_theme(lv_obj_t* slider, bool dark_mode) {
  if (!slider) {
    return;
  }

  const auto colors = palette(dark_mode);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, colors.button, LV_PART_MAIN);
  lv_obj_set_style_border_width(slider, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(slider, colors.border, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);

  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, colors.primary, LV_PART_INDICATOR);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_bg_color(slider, colors.primary_hover, LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 2, LV_PART_KNOB);
  lv_obj_set_style_border_color(slider, colors.surface, LV_PART_KNOB);
}

void apply_chart_theme(lv_obj_t* chart, lv_chart_series_t* series, bool dark_mode) {
  if (!chart) {
    return;
  }

  const auto colors = palette(dark_mode);
  lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(chart, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(chart, colors.border, LV_PART_MAIN);
  lv_obj_set_style_radius(chart, 4, LV_PART_MAIN);
  lv_obj_set_style_line_color(chart, colors.border, LV_PART_MAIN);
  lv_obj_set_style_line_opa(chart, LV_OPA_30, LV_PART_MAIN);

  if (series) {
    lv_chart_set_series_color(chart, series, colors.info);
  }
}

void set_style_color(lv_obj_t* obj, lv_style_prop_t prop, lv_color_t color) {
  if (!obj) {
    return;
  }

  switch (prop) {
    case LV_STYLE_BG_COLOR:
      lv_obj_set_style_bg_color(obj, color, 0);
      break;
    case LV_STYLE_BORDER_COLOR:
      lv_obj_set_style_border_color(obj, color, 0);
      break;
    case LV_STYLE_TEXT_COLOR:
      lv_obj_set_style_text_color(obj, color, 0);
      break;
    default:
      break;
  }
}

lv_color_t get_style_color(lv_obj_t* obj, lv_style_prop_t prop) {
  if (!obj) {
    return lv_color_black();
  }

  switch (prop) {
    case LV_STYLE_BG_COLOR:
      return lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
    case LV_STYLE_BORDER_COLOR:
      return lv_obj_get_style_border_color(obj, LV_PART_MAIN);
    case LV_STYLE_TEXT_COLOR:
      return lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    default:
      return lv_color_black();
  }
}

void animate_style_color(lv_obj_t* obj,
                         lv_style_prop_t prop,
                         lv_color_t target,
                         uint32_t duration_ms) {
  if (!obj) {
    return;
  }

  const auto from = get_style_color(obj, prop);
  if (duration_ms == 0 || lv_color_eq(from, target)) {
    set_style_color(obj, prop, target);
    return;
  }

  auto* data = new StyleColorAnim{prop, from, target};
  lv_anim_t anim{};
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, obj);
  lv_anim_set_user_data(&anim, data);
  lv_anim_set_values(&anim, 0, 255);
  lv_anim_set_duration(&anim, duration_ms);
  lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
  lv_anim_set_custom_exec_cb(&anim, style_color_anim_exec);
  lv_anim_set_deleted_cb(&anim, style_color_anim_deleted);
  lv_anim_start(&anim);
}

}  // namespace view
