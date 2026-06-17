/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "audio_test_page.h"

#include <chrono>
#include <cstdint>
#include <thread>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"

namespace screen {
namespace {

constexpr int K_RECORD_SECONDS         = 5;
constexpr const char* K_RECORDING_PATH = "/tmp/factory_audio_test.wav";
constexpr uint32_t K_LEVEL_POINT_COUNT = 42;

}  // namespace

AudioTestPage::AudioTestPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
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
  if (theme_observer_handle_) {
    lv_observer_remove(theme_observer_handle_);
    theme_observer_handle_ = nullptr;
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
  auto* status_font = assets_ref_().load_font("inter-semibold.ttf", 18);
  lv_obj_set_style_text_font(status_label_, status_font ? status_font : &lv_font_montserrat_18, 0);
  reactive::bind_theme(status_label_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  level_chart_ = lv_chart_create(group);
  lv_obj_remove_style_all(level_chart_);
  lv_obj_set_size(level_chart_, 250, 54);
  lv_chart_set_type(level_chart_, LV_CHART_TYPE_LINE);
  lv_chart_set_update_mode(level_chart_, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_point_count(level_chart_, K_LEVEL_POINT_COUNT);
  lv_chart_set_range(level_chart_, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_div_line_count(level_chart_, 3, 0);
  level_series_ = lv_chart_add_series(level_chart_,
                                      view::palette(app_view_model_ref_().is_dark_mode()).info,
                                      LV_CHART_AXIS_PRIMARY_Y);
  apply_level_chart_theme_(app_view_model_ref_().is_dark_mode());
  theme_observer_handle_ = reactive::observe_obj(level_chart_,
                                                 app_view_model_ref_().dark_mode_subject(),
                                                 theme_observer,
                                                 this);
  for (uint32_t i = 0; i < K_LEVEL_POINT_COUNT; ++i) {
    lv_chart_set_next_value(level_chart_, level_series_, 0);
  }

  error_label_ = lv_label_create(group);
  lv_obj_set_width(error_label_, 280);
  lv_obj_set_style_text_align(error_label_, LV_TEXT_ALIGN_CENTER, 0);
  auto* device_font = assets_ref_().load_font("inter-medium.ttf", 12);
  lv_obj_set_style_text_font(error_label_, device_font ? device_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(error_label_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  if (has_audio_device_) {
    lv_label_set_text(status_label_, "Press 6 or Enter to start recording");
    lv_label_set_text(error_label_, "");
    lv_obj_add_flag(error_label_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(status_label_, "Audio device error");
    lv_label_set_text(error_label_, device_error_.c_str());
    lv_obj_add_flag(level_chart_, LV_OBJ_FLAG_HIDDEN);
  }
}

void AudioTestPage::apply_level_chart_theme_(bool dark_mode) {
  view::apply_chart_theme(level_chart_, level_series_, dark_mode);
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
  job_state_->success.store(false);
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
    state->success.store(played);
    state->stage.store(played ? JobStage::COMPLETE : JobStage::ERROR);
  }).detach();
}

void AudioTestPage::update_status_() {
  if (!status_label_) {
    return;
  }

  if (!has_audio_device_) {
    lv_label_set_text(status_label_, "Audio device error");
    return;
  }

  const auto stage = job_state_ ? job_state_->stage.load() : JobStage::IDLE;
  if (stage == JobStage::RECORDING || stage == JobStage::PLAYING) {
    update_level_chart_();
  } else if (level_chart_ && level_series_) {
    lv_chart_set_next_value(level_chart_, level_series_, 0);
  }
  switch (stage) {
    case JobStage::IDLE:
      lv_label_set_text(status_label_, "Press 6 or Enter to start recording");
      break;
    case JobStage::RECORDING: {
      const uint32_t elapsed_ms = lv_tick_elaps(recording_started_at_);
      int remaining             = K_RECORD_SECONDS - static_cast<int>(elapsed_ms / 1000U);
      if (remaining < 0) {
        remaining = 0;
      }
      char buffer[32];
      lv_snprintf(buffer, sizeof(buffer), "Recording (%d)", remaining);
      lv_label_set_text(status_label_, buffer);
      break;
    }
    case JobStage::WAITING_TO_PLAY:
      lv_label_set_text(status_label_, "Recording (0)");
      break;
    case JobStage::PLAYING:
      lv_label_set_text(status_label_, "Playing");
      break;
    case JobStage::COMPLETE:
      lv_label_set_text(status_label_, "Does record and play normal?");
      break;
    case JobStage::ERROR:
      lv_label_set_text(status_label_, "Audio test failed. Press 6 or Enter to retry.");
      if (error_label_) {
        lv_label_set_text(error_label_, "See logs for PipeWire capture/playback details.");
        lv_obj_remove_flag(error_label_, LV_OBJ_FLAG_HIDDEN);
      }
      break;
  }
}

void AudioTestPage::update_level_chart_() {
  if (!level_chart_ || !level_series_) {
    return;
  }

  const int32_t level = static_cast<int32_t>(platform::audio::current_audio_level() * 100.0F);
  lv_chart_set_next_value(level_chart_, level_series_, level);
}

void AudioTestPage::theme_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<AudioTestPage*>(lv_observer_get_user_data(observer));
  if (page) {
    page->apply_level_chart_theme_(lv_subject_get_int(subject) != 0);
  }
}

void AudioTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  LV_UNUSED(key_name);

  auto* page = static_cast<AudioTestPage*>(user_data);
  if (!page) {
    return;
  }

  if (key == '6' || key == LV_KEY_ENTER) {
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
