/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "audio_service.h"
#include "connectivity_test_viewmodel.h"
#include "keyboard_test_viewmodel.h"
#include "lcd_test_viewmodel.h"
#include "linux_input.h"
#include "logger.h"
#include "screen_manager.h"
#include "start_menu_viewmodel.h"
#include "theme.h"

#if !USE_DESKTOP
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card0"
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

void global_key_listener(uint32_t key, const char* key_name, bool long_pressed, void* user_data) {
  LV_UNUSED(key_name);

  auto* app_view_model = static_cast<viewmodel::AppViewModel*>(user_data);
  if (!app_view_model || !is_theme_toggle_key(key)) {
    return;
  }

  const bool keyboard_page = app_view_model->current_page() == model::AppPage::KEYBOARD_TEST;
  if ((keyboard_page && long_pressed) || (!keyboard_page && !long_pressed)) {
    app_view_model->toggle_dark_mode();
    LOG_DEBUG("theme toggled from {} T key, dark_mode={}",
              keyboard_page ? "keyboard long-press" : "global",
              app_view_model->is_dark_mode());
  }
}

lv_display_t* init_display() {
#if USE_DESKTOP
  auto* display = lv_sdl_window_create(view::K_SCREEN_WIDTH, view::K_SCREEN_HEIGHT);
  if (!display) {
    return nullptr;
  }
  lv_sdl_window_set_title(display, "CardputerZero Factory Test");
  lv_sdl_window_set_resizeable(display, false);
  lv_sdl_mouse_create();
  lv_sdl_mousewheel_create();
  auto* keyboard = lv_sdl_keyboard_create();
  platform::attach_key_router(keyboard);
  return display;
#elif APP_USE_DRM
  auto* display = lv_linux_drm_create();
  if (!display) {
    return nullptr;
  }

  if (lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
    lv_display_delete(display);
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
  view::apply_lvgl_theme(display, app_view_model.is_dark_mode());
  platform::set_global_key_listener(global_key_listener, &app_view_model);

  ScreenManager screen_manager(app_view_model,
                               start_menu_view_model,
                               keyboard_view_model,
                               lcd_view_model,
                               connectivity_view_model,
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
