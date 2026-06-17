/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "audio_service.h"
#include "base_screen.h"

namespace screen {

class AudioTestPage : public BaseScreen {
 public:
  AudioTestPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~AudioTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  enum class JobStage {
    IDLE,
    RECORDING,
    WAITING_TO_PLAY,
    PLAYING,
    COMPLETE,
    ERROR,
  };

  struct AudioJobState {
    std::atomic<JobStage> stage{JobStage::IDLE};
    std::atomic<bool> success{false};
  };

  void start_recording_();
  void apply_level_chart_theme_(bool dark_mode);
  void update_status_();
  void update_level_chart_();
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void theme_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void poll_timer_cb(lv_timer_t* timer);

  platform::audio::AudioDevice device_{};
  std::string device_error_{};
  std::shared_ptr<AudioJobState> job_state_{};
  lv_obj_t* status_label_{nullptr};
  lv_obj_t* error_label_{nullptr};
  lv_obj_t* level_chart_{nullptr};
  lv_chart_series_t* level_series_{nullptr};
  lv_timer_t* poll_timer_{nullptr};
  lv_observer_t* theme_observer_handle_{nullptr};
  uint32_t recording_started_at_{0};
  bool has_audio_device_{false};
};

}  // namespace screen
