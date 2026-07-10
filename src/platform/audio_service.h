/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace platform::audio {

struct AudioDevice {
  std::string backend_name;
  std::string display_name;
  std::string playback_device;
  std::string capture_device;
};

bool find_audio_device(AudioDevice& device, std::string& error_message);
bool record_wav(const AudioDevice& device, const std::string& output_path, int seconds);
bool play_wav(const AudioDevice& device, const std::string& input_path);
void set_key_click_sound_path(const std::string& input_path);
void play_key_click_sound();

}  // namespace platform::audio
