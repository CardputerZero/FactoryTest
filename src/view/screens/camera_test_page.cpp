/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "camera_test_page.h"

#include <src/core/lv_obj_style_gen.h>
#include <src/lv_api_map_v8.h>

#include <algorithm>
#include <cstdint>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"

namespace screen {
namespace {

constexpr int32_t K_PREVIEW_WIDTH  = 226;
constexpr int32_t K_PREVIEW_HEIGHT = 170;

}  // namespace

CameraTestPage::CameraTestPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  set_default_test_nav_();
  has_camera_ = platform::camera::find_mipi_csi_camera(camera_, error_message_);
  if (has_camera_) {
    preview_started_ = preview_.start(camera_, error_message_);
    if (!preview_started_) {
      has_camera_ = false;
    }
  }
  init();
}

CameraTestPage::~CameraTestPage() {
  if (frame_timer_) {
    lv_timer_delete(frame_timer_);
    frame_timer_ = nullptr;
  }
  preview_.stop();
}

void CameraTestPage::build_content(lv_obj_t* content) {
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);

  if (has_camera_ && preview_started_) {
    preview_frame_ = lv_obj_create(root());
    lv_obj_remove_style_all(preview_frame_);
    lv_obj_set_size(preview_frame_, K_PREVIEW_WIDTH, K_PREVIEW_HEIGHT);
    lv_obj_set_style_bg_opa(preview_frame_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(preview_frame_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(preview_frame_, 0, 0);
    lv_obj_center(preview_frame_);
    lv_obj_move_to_index(preview_frame_, 0);

    preview_buffer_.assign(K_PREVIEW_WIDTH * K_PREVIEW_HEIGHT, 0);
    preview_image_dsc_.header.magic  = LV_IMAGE_HEADER_MAGIC;
    preview_image_dsc_.header.cf     = LV_COLOR_FORMAT_RGB565;
    preview_image_dsc_.header.flags  = LV_IMAGE_FLAGS_MODIFIABLE;
    preview_image_dsc_.header.w      = K_PREVIEW_WIDTH;
    preview_image_dsc_.header.h      = K_PREVIEW_HEIGHT;
    preview_image_dsc_.header.stride = K_PREVIEW_WIDTH * sizeof(uint16_t);
    preview_image_dsc_.data_size     = preview_buffer_.size() * sizeof(uint16_t);
    preview_image_dsc_.data          = reinterpret_cast<const uint8_t*>(preview_buffer_.data());

    preview_image_ = lv_image_create(preview_frame_);
    lv_obj_remove_style_all(preview_image_);
    lv_image_set_src(preview_image_, &preview_image_dsc_);
    lv_obj_center(preview_image_);

    update_preview_frame_();
    frame_timer_ = lv_timer_create(frame_timer_cb, 33, this);
    return;
  }

  auto* group = lv_obj_create(content);
  lv_obj_remove_style_all(group);
  lv_obj_set_size(group, 290, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(group, 10, 0);
  lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(group);

  error_title_ = lv_label_create(group);
  lv_obj_set_width(error_title_, 280);
  lv_obj_set_style_text_align(error_title_, LV_TEXT_ALIGN_CENTER, 0);
  auto* title_font = assets_ref_().load_font("inter-semibold.ttf", 18);
  lv_obj_set_style_text_font(error_title_, title_font ? title_font : &lv_font_montserrat_18, 0);
  reactive::bind_theme(error_title_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);
  lv_label_set_text(error_title_, "Camera device error");

  error_body_ = lv_label_create(group);
  lv_obj_set_width(error_body_, 280);
  lv_obj_set_style_text_align(error_body_, LV_TEXT_ALIGN_CENTER, 0);
  auto* body_font = assets_ref_().load_font("inter-medium.ttf", 13);
  lv_obj_set_style_text_font(error_body_, body_font ? body_font : &lv_font_montserrat_14, 0);
  reactive::bind_theme(error_body_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);
  lv_label_set_text(
      error_body_,
      error_message_.empty() ? "MIPI-CSI camera not detected." : error_message_.c_str());
}

void CameraTestPage::update_preview_frame_() {
  if (!preview_image_ || preview_buffer_.empty()) {
    return;
  }

  std::vector<uint16_t> frame;
  int width  = 0;
  int height = 0;
  if (!preview_.copy_frame_rgb565(frame, width, height) || width != K_PREVIEW_WIDTH ||
      height != K_PREVIEW_HEIGHT || frame.size() != preview_buffer_.size()) {
    return;
  }

  std::copy(frame.begin(), frame.end(), preview_buffer_.begin());
  preview_image_dsc_.data = reinterpret_cast<const uint8_t*>(preview_buffer_.data());
  lv_obj_invalidate(preview_image_);
}

void CameraTestPage::frame_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<CameraTestPage*>(lv_timer_get_user_data(timer));
  if (!page) {
    return;
  }
  page->update_preview_frame_();
}

}  // namespace screen
