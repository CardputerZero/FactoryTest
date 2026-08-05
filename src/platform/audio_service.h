/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <string>

namespace platform::audio {

struct AudioDevice {
  std::string backend_name;
  std::string display_name;
  std::string playback_device;
  std::string capture_device;
};

enum class UiSound : std::size_t {
  PRESS,
  SELECT,
  OPEN,
  CLOSE,
  SUCCESS,
  ERROR,
  WARNING,
  START,
  COMPLETE,
  TOGGLE_ON,
  COUNT,
};

bool find_audio_device(AudioDevice& device, std::string& error_message);
bool record_wav(const AudioDevice& device, const std::string& output_path, int seconds);
bool play_wav(const AudioDevice& device, const std::string& input_path);
void set_volume_level(float level);
const char* ui_sound_asset_filename(UiSound sound);
void set_ui_sounds_enabled(bool enabled);
bool ui_sounds_enabled();
void set_ui_sounds_volume_level(float level);
void set_ui_sound_path(UiSound sound, const std::string& input_path);
bool initialize_ui_sounds();
bool play_ui_sound(UiSound sound);
void stop_ui_sounds();
void shutdown_ui_sounds();

}  // namespace platform::audio
