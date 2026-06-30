/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "base_screen.h"

namespace screen {

class FtlEffectPage : public BaseScreen {
 public:
  FtlEffectPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~FtlEffectPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  static constexpr std::size_t K_STAR_COUNT = 80;
  static constexpr std::size_t K_STREAK_COUNT = 36;
  static constexpr int32_t K_CENTER_X       = 160;
  static constexpr int32_t K_CENTER_Y       = 85;

  struct Star {
    lv_obj_t* point{nullptr};
    lv_obj_t* line{nullptr};
    std::array<lv_point_precise_t, 2> line_points{};
    int32_t x{0};
    int32_t y{0};
    int32_t vx{0};
    int32_t vy{0};
    int32_t dx{0};
    int32_t dy{0};
    uint16_t spawn_delay_ms{0};
    uint16_t speed{0};
    uint16_t max_length{0};
    uint8_t brightness{0};
    uint8_t size{1};
  };

  void build_fullscreen_();
  void seed_scene_();
  void reset_star_(Star& star, bool anywhere);
  void reset_streak_(Star& star);
  void start_jump_();
  void finish_jump_();
  void update_normal_star_(Star& star);
  void update_jump_star_(Star& star, uint32_t elapsed_ms);
  void show_flash_(uint32_t elapsed_ms);
  uint32_t visual_elapsed_(uint32_t elapsed_ms) const;
  uint32_t next_random_();
  int32_t random_range_(int32_t min, int32_t max);

  static bool back_request_handler(void* user_data);
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void frame_timer_cb(lv_timer_t* timer);

  lv_obj_t* scene_{nullptr};
  lv_obj_t* flash_{nullptr};
  lv_timer_t* frame_timer_{nullptr};
  std::array<Star, K_STAR_COUNT> stars_{};
  uint32_t random_state_{0xC0FFEEU};
  uint32_t jump_started_at_{0};
  bool jumping_{false};
  bool looping_{false};
};

}  // namespace screen
