/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <vector>

#include "base_screen.h"
#include "camera_service.h"

namespace screen {

class CameraTestPage : public BaseScreen {
 public:
  CameraTestPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~CameraTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  void update_preview_frame_();
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void frame_timer_cb(lv_timer_t* timer);

  platform::camera::CameraInfo camera_{};
  platform::camera::PreviewSession preview_{};
  std::string error_message_{};
  lv_obj_t* preview_frame_{nullptr};
  lv_obj_t* preview_image_{nullptr};
  lv_obj_t* error_title_{nullptr};
  lv_obj_t* error_body_{nullptr};
  lv_image_dsc_t preview_image_dsc_{};
  std::vector<uint16_t> preview_buffer_{};
  lv_timer_t* frame_timer_{nullptr};
  bool has_camera_{false};
  bool preview_started_{false};
};

}  // namespace screen
