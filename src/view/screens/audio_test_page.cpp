/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "audio_test_page.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"

namespace screen {
namespace {

constexpr int K_RECORD_SECONDS         = 5;
constexpr const char* K_RECORDING_PATH = "/tmp/factory_audio_test.wav";

bool is_enter_key(uint32_t key, const char* key_name) {
  if (key == LV_KEY_ENTER || key == '\r') {
    return true;
  }
  return key_name && (std::strcmp(key_name, "Enter") == 0 || std::strcmp(key_name, "Return") == 0);
}

}  // namespace

AudioTestPage::AudioTestPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  set_default_test_nav_();
  has_audio_device_ = platform::audio::find_i2s_audio_device(device_, device_error_);
  job_state_        = std::make_shared<AudioJobState>();
  init();
  platform::set_key_listener(key_listener, this);
  poll_timer_ = lv_timer_create(poll_timer_cb, 100, this);
}

AudioTestPage::~AudioTestPage() {
  platform::clear_key_listener(key_listener, this);
  if (poll_timer_) {
    lv_timer_delete(poll_timer_);
    poll_timer_ = nullptr;
  }
}

void AudioTestPage::build_content(lv_obj_t* content) {
  auto* group = lv_obj_create(content);
  lv_obj_remove_style_all(group);
  lv_obj_set_size(group, 290, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(group, 10, 0);
  lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(group);

  status_label_ = lv_label_create(group);
  lv_obj_set_width(status_label_, 280);
  lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
  auto* status_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-semibold.ttf"), 18);
  lv_obj_set_style_text_font(status_label_, status_font ? status_font : &lv_font_montserrat_18, 0);
  reactive::bind_theme(status_label_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  if (has_audio_device_) {
    const auto text = app_view_model_ref_().tr("Press Enter to start recording");
    lv_label_set_text(status_label_, text.c_str());
  } else {
    const auto text = app_view_model_ref_().tr("Audio device error");
    lv_label_set_text(status_label_, text.c_str());
  }
}

void AudioTestPage::start_recording_() {
  if (!has_audio_device_ || !job_state_) {
    return;
  }
  const auto stage = job_state_->stage.load();
  if (stage == JobStage::RECORDING || stage == JobStage::WAITING_TO_PLAY ||
      stage == JobStage::PLAYING) {
    return;
  }

  recording_started_at_ = lv_tick_get();
  job_state_->stage.store(JobStage::RECORDING);
  update_status_();

  auto state        = job_state_;
  const auto device = device_;
  std::thread([state, device]() {
    const bool recorded = platform::audio::record_wav(device, K_RECORDING_PATH, K_RECORD_SECONDS);
    if (!recorded) {
      state->stage.store(JobStage::ERROR);
      return;
    }

    state->stage.store(JobStage::WAITING_TO_PLAY);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    state->stage.store(JobStage::PLAYING);

    const bool played = platform::audio::play_wav(device, K_RECORDING_PATH);
    state->stage.store(played ? JobStage::COMPLETE : JobStage::ERROR);
  }).detach();
}

void AudioTestPage::update_status_() {
  if (!status_label_) {
    return;
  }

  if (!has_audio_device_) {
    const auto text = app_view_model_ref_().tr("Audio device error");
    lv_label_set_text(status_label_, text.c_str());
    return;
  }

  const auto stage = job_state_ ? job_state_->stage.load() : JobStage::IDLE;
  std::string text;
  switch (stage) {
    case JobStage::IDLE:
      text = app_view_model_ref_().tr("Press Enter to start recording");
      break;
    case JobStage::RECORDING: {
      const uint32_t elapsed_ms = lv_tick_elaps(recording_started_at_);
      int remaining             = K_RECORD_SECONDS - static_cast<int>(elapsed_ms / 1000U);
      if (remaining < 0) {
        remaining = 0;
      }
      char buffer[32];
      lv_snprintf(buffer,
                  sizeof(buffer),
                  "%s (%d)",
                  app_view_model_ref_().tr("Recording").c_str(),
                  remaining);
      lv_label_set_text(status_label_, buffer);
      break;
    }
    case JobStage::WAITING_TO_PLAY:
      text = app_view_model_ref_().tr("Recording complete");
      break;
    case JobStage::PLAYING:
      text = app_view_model_ref_().tr("Playing");
      break;
    case JobStage::COMPLETE:
      text = app_view_model_ref_().tr("Playback complete");
      break;
    case JobStage::ERROR:
      text = app_view_model_ref_().tr("Audio test failed. Press Enter to retry.");
      break;
  }
  if (!text.empty()) {
    lv_label_set_text(status_label_, text.c_str());
  }
}

void AudioTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<AudioTestPage*>(user_data);
  if (!page) {
    return;
  }
  if (page->handle_test_result_dialog_key_(key, key_name)) {
    return;
  }

  if (is_enter_key(key, key_name)) {
    page->start_recording_();
  }
}

void AudioTestPage::poll_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<AudioTestPage*>(lv_timer_get_user_data(timer));
  if (!page) {
    return;
  }

  page->update_status_();
}

}  // namespace screen
