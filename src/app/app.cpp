/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"

#include <cstddef>
#include <cstring>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "audio_service.h"
#include "connectivity_test_viewmodel.h"
#include "keyboard_test_viewmodel.h"
#include "lcd_test_viewmodel.h"
#include "linux_input.h"
#include "logger.h"
#include "perf_test_viewmodel.h"
#include "screen_manager.h"
#include "screenshot_service.h"
#include "start_menu_viewmodel.h"
#include "theme.h"

#if !USE_DESKTOP
#if APP_USE_DRM
#include <unistd.h>

#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card1"
#endif

#ifndef APP_DRM_CONNECTOR_ID
#define APP_DRM_CONNECTOR_ID (-1)
#endif

namespace app {
namespace {

void quit_requested_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* running = static_cast<bool*>(lv_observer_get_user_data(observer));
  if (running && lv_subject_get_int(subject)) {
    *running = false;
  }
}

void dark_mode_observer(lv_observer_t* observer, lv_subject_t* subject) {
  LV_UNUSED(observer);
  LV_UNUSED(subject);
  // Object-level theme bindings animate their own colors; avoid a full display theme reset.
}

bool is_theme_toggle_key(uint32_t key) { return key == 't' || key == 'T'; }

bool is_screenshot_key(uint32_t key) { return key == 'p' || key == 'P'; }

#if !USE_DESKTOP && APP_USE_DRM
bool is_duplicate_candidate(const char* candidate,
                            const char* const* candidates,
                            std::size_t count) {
  if (!candidate || candidate[0] == '\0') {
    return true;
  }

  for (std::size_t i = 0; i < count; ++i) {
    if (candidates[i] && std::strcmp(candidate, candidates[i]) == 0) {
      return true;
    }
  }
  return false;
}

lv_display_t* try_drm_display(const char* device_path) {
  if (!device_path || device_path[0] == '\0') {
    return nullptr;
  }

  if (access(device_path, R_OK | W_OK) != 0) {
    LOG_DEBUG("DRM display device is not accessible: {}", device_path);
    return nullptr;
  }

  auto* display = lv_linux_drm_create();
  if (!display) {
    LOG_ERROR("failed to create DRM display object for {}", device_path);
    return nullptr;
  }

  LOG_INFO("trying DRM display device: {} connector={}", device_path, APP_DRM_CONNECTOR_ID);
  if (lv_linux_drm_set_file(display, device_path, APP_DRM_CONNECTOR_ID) == LV_RESULT_OK) {
    LOG_INFO("using DRM display device: {} connector={}", device_path, APP_DRM_CONNECTOR_ID);
    return display;
  }

  lv_display_delete(display);
  LOG_WARN("DRM display device failed: {}", device_path);
  return nullptr;
}

lv_display_t* init_drm_display() {
  const char* candidates[] = {
      APP_DRM_DEVICE,
      "/dev/dri/card1",
      "/dev/dri/card0",
      "/dev/dri/card2",
      "/dev/dri/card3",
  };
  const char* tried[sizeof(candidates) / sizeof(candidates[0])] = {};
  std::size_t tried_count                                       = 0;

  for (const char* candidate : candidates) {
    if (is_duplicate_candidate(candidate, tried, tried_count)) {
      continue;
    }
    tried[tried_count++] = candidate;

    if (auto* display = try_drm_display(candidate)) {
      return display;
    }
  }

  return nullptr;
}
#endif

void global_key_listener(uint32_t key, const char* key_name, bool long_pressed, void* user_data) {
  LV_UNUSED(key_name);

  auto* app_view_model = static_cast<viewmodel::AppViewModel*>(user_data);
  if (!app_view_model) {
    return;
  }

  const bool keyboard_page = app_view_model->current_page() == model::AppPage::KEYBOARD_TEST;
  const bool connectivity_page =
      app_view_model->current_page() == model::AppPage::CONNECTIVITY_TEST;
  const bool should_handle = (keyboard_page && long_pressed) || (!keyboard_page && !long_pressed);
  if (is_theme_toggle_key(key) && should_handle && !connectivity_page) {
    app_view_model->toggle_dark_mode();
    LOG_DEBUG("theme toggled from {} T key, dark_mode={}",
              keyboard_page ? "keyboard long-press" : "global",
              app_view_model->is_dark_mode());
  } else if (is_screenshot_key(key) && should_handle && !connectivity_page) {
    platform::screenshot::capture_active_screen_with_overlay();
  }
}

lv_display_t* init_display() {
#if USE_DESKTOP
  auto* display = lv_sdl_window_create(view::K_SCREEN_WIDTH, view::K_SCREEN_HEIGHT);
  if (!display) {
    return nullptr;
  }
  lv_sdl_window_set_title(display, "Factory Test");
  lv_sdl_window_set_resizeable(display, false);
  lv_sdl_mouse_create();
  lv_sdl_mousewheel_create();
  auto* keyboard = lv_sdl_keyboard_create();
  platform::attach_key_router(keyboard);
  return display;
#elif APP_USE_DRM
  auto* display = init_drm_display();
  if (!display) {
    return nullptr;
  }

  platform::init_key_input(display);
  return display;
#else
  auto* display = lv_linux_fbdev_create();
  if (!display) {
    return nullptr;
  }

  if (lv_linux_fbdev_set_file(display, APP_FRAMEBUFFER_DEVICE) != LV_RESULT_OK) {
    lv_display_delete(display);
    return nullptr;
  }

  platform::init_key_input(display);
  return display;
#endif
}

}  // namespace

int Application::run() {
  logger::Logger::init();
  logger::Logger::set_tag("factory-test");

  lv_init();

  auto* display = init_display();
  if (!display) {
    LOG_ERROR("failed to initialize display");
    return 1;
  }

  AssetManager assets;
  for (const auto& root : assets.roots()) {
    LOG_INFO("asset root: {}", root.string());
  }
  const auto click_sound_path = assets.resolve("audio/click.wav");
  if (!click_sound_path.empty()) {
    platform::audio::set_key_click_sound_path(click_sound_path.string());
  } else {
    LOG_WARN("key click sound asset not found: audio/click.wav");
  }

  viewmodel::AppViewModel app_view_model;
  viewmodel::StartMenuViewModel start_menu_view_model;
  viewmodel::KeyboardTestViewModel keyboard_view_model;
  viewmodel::LcdTestViewModel lcd_view_model;
  viewmodel::ConnectivityTestViewModel connectivity_view_model;
  viewmodel::PerfTestViewModel perf_view_model;
  view::apply_lvgl_theme(display, app_view_model.is_dark_mode());
  platform::set_global_key_listener(global_key_listener, &app_view_model);

  ScreenManager screen_manager(app_view_model,
                               start_menu_view_model,
                               keyboard_view_model,
                               lcd_view_model,
                               connectivity_view_model,
                               perf_view_model,
                               assets);
  screen_manager.start();

  bool running        = true;
  auto* quit_observer = lv_subject_add_observer(app_view_model.quit_requested_subject(),
                                                quit_requested_observer,
                                                &running);
  auto* dark_mode_observer_handle =
      lv_subject_add_observer(app_view_model.dark_mode_subject(), dark_mode_observer, display);

  LOG_INFO("LVGL app started at {}x{}",
           lv_display_get_horizontal_resolution(display),
           lv_display_get_vertical_resolution(display));
  while (running) {
    lv_timer_handler();
    lv_delay_ms(5);
  }

  if (quit_observer) {
    lv_observer_remove(quit_observer);
  }
  if (dark_mode_observer_handle) {
    lv_observer_remove(dark_mode_observer_handle);
  }
  platform::clear_global_key_listener(global_key_listener, &app_view_model);

  return 0;
}

}  // namespace app
