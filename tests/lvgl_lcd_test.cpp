/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <cstdlib>

#include "lvgl.h"
#include "src/drivers/display/drm/lv_linux_drm.h"

#ifndef TEST_LCD_DRM_DEVICE
#define TEST_LCD_DRM_DEVICE "/dev/dri/cardputer-zero-internal"
#endif

int main() {
  lv_init();

  auto* display = lv_linux_drm_create();
  if (!display) {
    std::fprintf(stderr, "lvgl_lcd_test: failed to create DRM display\n");
    return 1;
  }

  if (lv_linux_drm_set_file(display, TEST_LCD_DRM_DEVICE, -1) != LV_RESULT_OK) {
    std::fprintf(stderr, "lvgl_lcd_test: failed to open %s\n", TEST_LCD_DRM_DEVICE);
    lv_display_delete(display);
    return 1;
  }

  const int32_t width  = lv_display_get_horizontal_resolution(display);
  const int32_t height = lv_display_get_vertical_resolution(display);
  std::printf("lvgl_lcd_test: opened %s resolution=%dx%d\n",
              TEST_LCD_DRM_DEVICE,
              width,
              height);

  // White background, black text.
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

  lv_obj_t* label = lv_label_create(screen);
  lv_label_set_text(label, "Hello from LCD");
  lv_obj_set_style_text_color(label, lv_color_black(), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
  lv_obj_center(label);

  lv_refr_now(display);

  for (;;) {
    lv_timer_handler();
    lv_delay_ms(5);
  }

  return 0;
}
