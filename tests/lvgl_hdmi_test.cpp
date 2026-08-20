/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

#include "lvgl.h"
#include "src/drivers/display/drm/lv_linux_drm.h"
#include "xf86drm.h"
#include "xf86drmMode.h"

#ifndef TEST_HDMI_DRM_DEVICE
#define TEST_HDMI_DRM_DEVICE "/dev/dri/card0"
#endif

namespace {

// Pick the first connected connector that carries HDMI EDID modes.  This is
// what carries the monitor's native resolution, matching how the factory
// precheck service selects its display.
int64_t find_connected_connector(int fd) {
  drmModeRes* resources = drmModeGetResources(fd);
  if (!resources) {
    return -1;
  }

  int64_t connector_id = -1;
  for (int i = 0; i < resources->count_connectors && connector_id < 0; ++i) {
    drmModeConnector* connector = drmModeGetConnector(fd, resources->connectors[i]);
    if (!connector) {
      continue;
    }
    if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
      connector_id = static_cast<int64_t>(connector->connector_id);
    }
    drmModeFreeConnector(connector);
  }

  drmModeFreeResources(resources);
  return connector_id;
}

// Report the native/preferred mode carried in the EDID so the test can be
// verified against what the monitor actually advertises.
void print_edid_mode(int fd, int64_t connector_id) {
  drmModeConnector* connector = drmModeGetConnector(fd, connector_id);
  if (!connector) {
    return;
  }

  if (connector->count_modes > 0) {
    drmModeModeInfo* preferred = &connector->modes[0];
    for (int i = 0; i < connector->count_modes; ++i) {
      if (connector->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
        preferred = &connector->modes[i];
        break;
      }
    }
    std::printf("lvgl_hdmi_test: EDID mode %s %dx%d@%dHz\n",
                preferred->name,
                preferred->hdisplay,
                preferred->vdisplay,
                preferred->vrefresh);
  }

  drmModeFreeConnector(connector);
}

}  // namespace

int main() {
  lv_init();

  auto* display = lv_linux_drm_create();
  if (!display) {
    std::fprintf(stderr, "lvgl_hdmi_test: failed to create DRM display\n");
    return 1;
  }

  const int fd = open(TEST_HDMI_DRM_DEVICE, O_RDWR | O_CLOEXEC);
  if (fd >= 0) {
    const int64_t connector_id = find_connected_connector(fd);
    if (connector_id >= 0) {
      print_edid_mode(fd, connector_id);
    }
    close(fd);
  }

  if (lv_linux_drm_set_file(display, TEST_HDMI_DRM_DEVICE, -1) != LV_RESULT_OK) {
    std::fprintf(stderr, "lvgl_hdmi_test: failed to open %s\n", TEST_HDMI_DRM_DEVICE);
    lv_display_delete(display);
    return 1;
  }

  // The LVGL DRM driver sizes the framebuffer from the EDID mode it selects,
  // so these values are the monitor's own resolution.
  const int32_t width  = lv_display_get_horizontal_resolution(display);
  const int32_t height = lv_display_get_vertical_resolution(display);
  std::printf("lvgl_hdmi_test: opened %s resolution=%dx%d\n",
              TEST_HDMI_DRM_DEVICE,
              width,
              height);

  // White background, black text.
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

  lv_obj_t* label = lv_label_create(screen);
  lv_label_set_text(label, "Hello from your decent monitor");
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
