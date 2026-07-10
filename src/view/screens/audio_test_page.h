/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <cstdint>
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
  };

  void start_recording_();
  void update_status_();
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void poll_timer_cb(lv_timer_t* timer);

  platform::audio::AudioDevice device_{};
  std::string device_error_{};
  std::shared_ptr<AudioJobState> job_state_{};
  lv_obj_t* status_label_{nullptr};
  lv_timer_t* poll_timer_{nullptr};
  uint32_t recording_started_at_{0};
  bool has_audio_device_{false};
};

}  // namespace screen
