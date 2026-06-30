/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ftl_effect_page.h"

#include <algorithm>
#include <cstdlib>

#include "linux_input.h"
#include "theme.h"

namespace screen {
namespace {

constexpr uint32_t K_FRAME_MS            = 33;
constexpr uint32_t K_JUMP_DURATION_MS    = 8200;
constexpr uint32_t K_STREAK_START_MS     = 180;
constexpr uint32_t K_FLASH_START_MS      = 7600;
constexpr uint32_t K_FLASH_PEAK_MS       = 7900;
constexpr uint32_t K_FLASH_END_MS        = 8200;
constexpr int32_t K_FIXED_SHIFT          = 8;
constexpr int32_t K_STREAK_PHASE_MS      = K_FLASH_START_MS - K_STREAK_START_MS;
constexpr int32_t K_STREAK_CYCLE_MS      = 2450;
constexpr int32_t K_STREAK_EDGE_RADIUS   = 205;
constexpr lv_opa_t K_HIDDEN_OPACITY      = LV_OPA_TRANSP;
constexpr uint32_t K_STREAK_REDSHIFT     = 0xFF5536;
constexpr uint32_t K_STREAK_WARM_WHITE   = 0xFFE6C8;
constexpr uint32_t K_STREAK_BLUESHIFT    = 0x7FF4FF;

lv_opa_t flash_opacity(uint32_t elapsed_ms) {
  if (elapsed_ms < K_FLASH_START_MS) {
    return LV_OPA_TRANSP;
  }
  if (elapsed_ms < K_FLASH_PEAK_MS) {
    const auto progress = static_cast<int32_t>(elapsed_ms - K_FLASH_START_MS);
    return static_cast<lv_opa_t>(std::min<int32_t>(255, progress * 255 /
                                                           (K_FLASH_PEAK_MS - K_FLASH_START_MS)));
  }
  if (elapsed_ms < K_FLASH_END_MS) {
    const auto progress = static_cast<int32_t>(elapsed_ms - K_FLASH_PEAK_MS);
    return static_cast<lv_opa_t>(
        std::max<int32_t>(0, 255 - progress * 255 / (K_FLASH_END_MS - K_FLASH_PEAK_MS)));
  }
  return LV_OPA_TRANSP;
}

lv_point_precise_t precise_point(int32_t x, int32_t y) {
  return {static_cast<lv_value_precise_t>(x), static_cast<lv_value_precise_t>(y)};
}

lv_color_t streak_color(int32_t local_ms, int32_t head_radius) {
  if (head_radius < 52) {
    return lv_color_mix(lv_color_hex(K_STREAK_WARM_WHITE),
                        lv_color_hex(K_STREAK_REDSHIFT),
                        72 + std::min<int32_t>(80, local_ms / 12));
  }
  if (head_radius < 128) {
    return lv_color_mix(lv_color_hex(K_STREAK_BLUESHIFT),
                        lv_color_hex(K_STREAK_WARM_WHITE),
                        86 + std::min<int32_t>(70, local_ms / 30));
  }
  return lv_color_mix(lv_color_white(),
                      lv_color_hex(K_STREAK_BLUESHIFT),
                      148 + std::min<int32_t>(72, (head_radius - 128) * 2));
}

int32_t streak_velocity_scale(int32_t global_ms) {
  const int32_t progress = std::clamp(global_ms * 1000 / K_STREAK_PHASE_MS, 0, 1000);
  if (progress < 280) {
    return 42 + progress * 108 / 280;
  }
  if (progress < 680) {
    return 150;
  }
  return 48 + (1000 - progress) * 102 / 320;
}

}  // namespace

FtlEffectPage::FtlEffectPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  app_view_model_ref_().clear_nav_actions();
  app_view_model_ref_().set_back_request_handler(back_request_handler, this);
  init();
  platform::set_key_listener(key_listener, this);
}

FtlEffectPage::~FtlEffectPage() {
  platform::clear_key_listener(key_listener, this);
  app_view_model_ref_().clear_back_request_handler(back_request_handler, this);
  if (frame_timer_) {
    lv_timer_delete(frame_timer_);
    frame_timer_ = nullptr;
  }
}

void FtlEffectPage::build_content(lv_obj_t* content) {
  LV_UNUSED(content);
  build_fullscreen_();
  seed_scene_();
  frame_timer_ = lv_timer_create(frame_timer_cb, K_FRAME_MS, this);
}

void FtlEffectPage::build_fullscreen_() {
  auto* screen = root();
  if (!screen) {
    return;
  }

  lv_obj_remove_style_all(screen);
  lv_obj_set_size(screen, view::K_SCREEN_WIDTH, view::K_SCREEN_HEIGHT);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  scene_ = lv_obj_create(screen);
  lv_obj_remove_style_all(scene_);
  lv_obj_set_size(scene_, view::K_SCREEN_WIDTH, view::K_SCREEN_HEIGHT);
  lv_obj_align(scene_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(scene_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(scene_, lv_color_black(), 0);
  lv_obj_clear_flag(scene_, LV_OBJ_FLAG_SCROLLABLE);

  flash_ = lv_obj_create(screen);
  lv_obj_remove_style_all(flash_);
  lv_obj_set_size(flash_, view::K_SCREEN_WIDTH, view::K_SCREEN_HEIGHT);
  lv_obj_align(flash_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(flash_, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(flash_, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(flash_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(flash_, LV_OBJ_FLAG_CLICKABLE);
}

void FtlEffectPage::seed_scene_() {
  for (std::size_t i = 0; i < stars_.size(); ++i) {
    auto& star = stars_[i];
    star.point = lv_obj_create(scene_);
    lv_obj_remove_style_all(star.point);
    lv_obj_set_style_radius(star.point, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(star.point, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(star.point, lv_color_white(), 0);
    lv_obj_clear_flag(star.point, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(star.point, LV_OBJ_FLAG_CLICKABLE);

    if (i < K_STREAK_COUNT) {
      star.line = lv_line_create(scene_);
      lv_obj_remove_style_all(star.line);
      lv_obj_set_size(star.line, view::K_SCREEN_WIDTH, view::K_SCREEN_HEIGHT);
      lv_obj_align(star.line, LV_ALIGN_TOP_LEFT, 0, 0);
      lv_obj_set_style_line_color(star.line, lv_color_hex(K_STREAK_WARM_WHITE), 0);
      lv_obj_set_style_line_width(star.line, 2, 0);
      lv_obj_set_style_line_rounded(star.line, true, 0);
      lv_obj_set_style_line_opa(star.line, K_HIDDEN_OPACITY, 0);
      lv_line_set_points_mutable(star.line, star.line_points.data(), star.line_points.size());
    }

    reset_star_(star, true);
    if (i < K_STREAK_COUNT) {
      reset_streak_(star);
    }
  }
}

void FtlEffectPage::reset_star_(Star& star, bool anywhere) {
  if (anywhere) {
    star.x = random_range_(0, view::K_SCREEN_WIDTH - 1) << K_FIXED_SHIFT;
    star.y = random_range_(0, view::K_SCREEN_HEIGHT - 1) << K_FIXED_SHIFT;
  } else {
    star.x = (K_CENTER_X + random_range_(-12, 12)) << K_FIXED_SHIFT;
    star.y = (K_CENTER_Y + random_range_(-8, 8)) << K_FIXED_SHIFT;
  }

  int32_t dx = (star.x >> K_FIXED_SHIFT) - K_CENTER_X;
  int32_t dy = (star.y >> K_FIXED_SHIFT) - K_CENTER_Y;
  if (dx == 0 && dy == 0) {
    dx = random_range_(-2, 2);
    dy = random_range_(-2, 2);
    if (dx == 0 && dy == 0) {
      dx = 1;
    }
  }
  star.vx         = dx;
  star.vy         = dy;
  star.brightness = static_cast<uint8_t>(random_range_(120, 255));
  star.size       = static_cast<uint8_t>(random_range_(0, 5) == 0 ? 2 : 1);

  if (star.point) {
    lv_obj_set_size(star.point, star.size, star.size);
  }
  update_normal_star_(star);
}

void FtlEffectPage::reset_streak_(Star& star) {
  int32_t dx = random_range_(-160, 160);
  int32_t dy = random_range_(-85, 85);
  if (std::abs(dx) < 12 && std::abs(dy) < 12) {
    dx = dx < 0 ? -24 : 24;
    dy = dy < 0 ? -16 : 16;
  }

  const int32_t denom = std::max({1, std::abs(dx), std::abs(dy)});
  star.dx            = dx * 256 / denom;
  star.dy            = dy * 256 / denom;
  star.spawn_delay_ms = static_cast<uint16_t>(random_range_(0, K_STREAK_CYCLE_MS - 1));
  star.speed          = static_cast<uint16_t>(random_range_(34, 58));
  star.max_length     = static_cast<uint16_t>(random_range_(70, 132));
  star.brightness     = static_cast<uint8_t>(random_range_(150, 255));

  if (star.line) {
    star.line_points[0] = precise_point(K_CENTER_X, K_CENTER_Y);
    star.line_points[1] = precise_point(K_CENTER_X, K_CENTER_Y);
    lv_obj_set_style_line_opa(star.line, K_HIDDEN_OPACITY, 0);
    lv_obj_set_style_line_width(star.line, random_range_(0, 3) == 0 ? 3 : 2, 0);
    lv_obj_invalidate(star.line);
  }
}

void FtlEffectPage::start_jump_() {
  if (jumping_) {
    return;
  }

  jumping_         = true;
  jump_started_at_ = lv_tick_get();
  for (std::size_t i = 0; i < stars_.size(); ++i) {
    auto& star = stars_[i];
    if (i < K_STREAK_COUNT) {
      reset_streak_(star);
    } else {
      reset_star_(star, true);
    }
    if (star.point) {
      lv_obj_set_style_bg_opa(star.point, LV_OPA_TRANSP, 0);
    }
    if (star.line) {
      lv_obj_set_style_line_opa(star.line, K_HIDDEN_OPACITY, 0);
    }
  }
}

void FtlEffectPage::finish_jump_() {
  jumping_ = false;
  lv_obj_set_style_bg_opa(flash_, LV_OPA_TRANSP, 0);
  for (auto& star : stars_) {
    if (star.line) {
      lv_obj_set_style_line_opa(star.line, K_HIDDEN_OPACITY, 0);
    }
    if (star.point) {
      lv_obj_set_style_bg_opa(star.point, LV_OPA_COVER, 0);
    }
    reset_star_(star, true);
  }
}

void FtlEffectPage::update_normal_star_(Star& star) {
  const int32_t x = star.x >> K_FIXED_SHIFT;
  const int32_t y = star.y >> K_FIXED_SHIFT;
  const auto color =
      lv_color_hex((static_cast<uint32_t>(star.brightness) << 16) |
                   (static_cast<uint32_t>(star.brightness) << 8) | star.brightness);
  if (star.point) {
    lv_obj_set_style_bg_color(star.point, color, 0);
    lv_obj_set_pos(star.point, x, y);
  }
  star.line_points[0] = precise_point(x, y);
  star.line_points[1] = precise_point(x, y);
  if (star.line) {
    lv_obj_invalidate(star.line);
  }
}

void FtlEffectPage::update_jump_star_(Star& star, uint32_t elapsed_ms) {
  if (elapsed_ms < K_STREAK_START_MS) {
    if (star.point) {
      lv_obj_set_style_bg_opa(star.point, LV_OPA_COVER, 0);
      update_normal_star_(star);
    }
    return;
  }

  if (star.point) {
    lv_obj_set_style_bg_opa(star.point, LV_OPA_TRANSP, 0);
  }
  if (!star.line) {
    return;
  }

  const int32_t global_ms = static_cast<int32_t>(elapsed_ms - K_STREAK_START_MS);
  const int32_t local_ms =
      (global_ms + K_STREAK_CYCLE_MS - star.spawn_delay_ms) % K_STREAK_CYCLE_MS;
  const int32_t velocity_scale = streak_velocity_scale(global_ms);
  const int32_t head_radius = std::min<int32_t>(
      K_STREAK_EDGE_RADIUS,
      1 + (local_ms * static_cast<int32_t>(star.speed) * velocity_scale) / 16500);
  const int32_t grow_length = std::min<int32_t>(star.max_length, 2 + local_ms / 12);
  const int32_t detach_delay = 720;
  const int32_t tail_radius =
      local_ms < detach_delay
          ? 0
          : std::min<int32_t>(K_STREAK_EDGE_RADIUS,
                              ((local_ms - detach_delay) * static_cast<int32_t>(star.speed) *
                               std::max<int32_t>(42, velocity_scale - 18)) /
                                  18500);
  const int32_t start_radius = std::max<int32_t>(tail_radius, head_radius - grow_length);

  const int32_t tail_x  = K_CENTER_X + star.dx * start_radius / 256;
  const int32_t tail_y  = K_CENTER_Y + star.dy * start_radius / 256;
  const int32_t head_x  = K_CENTER_X + star.dx * head_radius / 256;
  const int32_t head_y  = K_CENTER_Y + star.dy * head_radius / 256;
  const int32_t edge_fade =
      head_radius > 155 ? std::max<int32_t>(0, 255 - (head_radius - 155) * 5) : 255;
  const int32_t cycle_fade = local_ms > K_STREAK_CYCLE_MS - 360
                                 ? std::max<int32_t>(
                                       0, (K_STREAK_CYCLE_MS - local_ms) * 255 / 360)
                                 : 255;
  const auto opa = static_cast<lv_opa_t>(
      std::min<int32_t>(cycle_fade,
                        std::min<int32_t>(edge_fade, std::min<int32_t>(235, 95 + local_ms / 12))));

  lv_obj_set_style_line_color(star.line, streak_color(local_ms, head_radius), 0);
  star.line_points[0] = precise_point(tail_x, tail_y);
  star.line_points[1] = precise_point(head_x, head_y);
  lv_obj_set_style_line_opa(star.line, opa, 0);
  lv_obj_invalidate(star.line);
}

void FtlEffectPage::show_flash_(uint32_t elapsed_ms) {
  if (!flash_) {
    return;
  }
  lv_obj_set_style_bg_opa(flash_, flash_opacity(elapsed_ms), 0);
  lv_obj_move_foreground(flash_);
}

uint32_t FtlEffectPage::visual_elapsed_(uint32_t elapsed_ms) const {
  if (!looping_) {
    return elapsed_ms;
  }

  constexpr uint32_t K_LOOP_HOLD_START_MS = 1250;
  constexpr uint32_t K_LOOP_HOLD_END_MS   = 5600;
  constexpr uint32_t K_LOOP_SPAN_MS       = K_LOOP_HOLD_END_MS - K_LOOP_HOLD_START_MS;
  if (elapsed_ms < K_LOOP_HOLD_START_MS) {
    return elapsed_ms;
  }
  return K_LOOP_HOLD_START_MS + ((elapsed_ms - K_LOOP_HOLD_START_MS) % K_LOOP_SPAN_MS);
}

uint32_t FtlEffectPage::next_random_() {
  random_state_ = random_state_ * 1664525U + 1013904223U;
  return random_state_;
}

int32_t FtlEffectPage::random_range_(int32_t min, int32_t max) {
  if (max <= min) {
    return min;
  }
  return min + static_cast<int32_t>(next_random_() % static_cast<uint32_t>(max - min + 1));
}

bool FtlEffectPage::back_request_handler(void* user_data) {
  auto* page = static_cast<FtlEffectPage*>(user_data);
  if (!page) {
    return false;
  }
  page->app_view_model_ref_().show_start_page();
  page->app_view_model_ref_().refresh_current_page();
  return true;
}

void FtlEffectPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  LV_UNUSED(key_name);

  auto* page = static_cast<FtlEffectPage*>(user_data);
  if (!page) {
    return;
  }

  if (key == LV_KEY_ESC || key == '4') {
    page->app_view_model_ref_().show_start_page();
    page->app_view_model_ref_().refresh_current_page();
    return;
  }

  if (key == LV_KEY_ENTER) {
    page->looping_ = false;
    page->start_jump_();
    return;
  }

  if (key == ' ') {
    if (!page->jumping_) {
      page->looping_ = true;
      page->start_jump_();
    }
  }
}

void FtlEffectPage::frame_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<FtlEffectPage*>(lv_timer_get_user_data(timer));
  if (!page) {
    return;
  }

  if (!page->jumping_) {
    return;
  }

  const auto elapsed = lv_tick_elaps(page->jump_started_at_);
  if (!page->looping_ && elapsed >= K_JUMP_DURATION_MS) {
    page->finish_jump_();
    return;
  }

  const auto visual_elapsed = page->visual_elapsed_(elapsed);
  const std::size_t animated_star_count = visual_elapsed < K_STREAK_START_MS ? page->stars_.size()
                                                                             : K_STREAK_COUNT;
  for (std::size_t i = 0; i < animated_star_count; ++i) {
    page->update_jump_star_(page->stars_[i], visual_elapsed);
  }
  if (page->looping_) {
    lv_obj_set_style_bg_opa(page->flash_, LV_OPA_TRANSP, 0);
  } else {
    page->show_flash_(visual_elapsed);
  }
}

}  // namespace screen
