/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "audio_service.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                                    \
  do {                                                                                      \
    if (!(condition)) {                                                                     \
      std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " << #condition << '\n'; \
      ++failures;                                                                           \
    }                                                                                       \
  } while (false)

struct ExpectedSound {
  platform::audio::UiSound sound;
  const char* filename;
};

constexpr std::array<ExpectedSound, 10> K_EXPECTED_SOUNDS = {{
    {platform::audio::UiSound::PRESS, "click.wav"},
    {platform::audio::UiSound::SELECT, "select.wav"},
    {platform::audio::UiSound::OPEN, "open.wav"},
    {platform::audio::UiSound::CLOSE, "close.wav"},
    {platform::audio::UiSound::SUCCESS, "success.wav"},
    {platform::audio::UiSound::ERROR, "error.wav"},
    {platform::audio::UiSound::WARNING, "warning.wav"},
    {platform::audio::UiSound::START, "start.wav"},
    {platform::audio::UiSound::COMPLETE, "complete.wav"},
    {platform::audio::UiSound::TOGGLE_ON, "toggle-on.wav"},
}};

bool is_pcm_wav(const std::filesystem::path& path) {
  std::array<char, 12> header{};
  std::ifstream input(path, std::ios::binary);
  input.read(header.data(), static_cast<std::streamsize>(header.size()));
  return input.gcount() == static_cast<std::streamsize>(header.size()) &&
         std::memcmp(header.data(), "RIFF", 4) == 0 &&
         std::memcmp(header.data() + 8, "WAVE", 4) == 0;
}

}  // namespace

int main() {
  const std::filesystem::path audio_root = TEST_AUDIO_ASSETS_ROOT;
  for (const auto& expected : K_EXPECTED_SOUNDS) {
    const char* filename = platform::audio::ui_sound_asset_filename(expected.sound);
    CHECK(filename != nullptr);
    CHECK(filename && std::string(filename) == expected.filename);
    const auto path = audio_root / expected.filename;
    CHECK(std::filesystem::is_regular_file(path));
    CHECK(std::filesystem::file_size(path) > 44);
    CHECK(is_pcm_wav(path));
  }
  CHECK(platform::audio::ui_sound_asset_filename(platform::audio::UiSound::COUNT) == nullptr);

  if (failures != 0) {
    std::cerr << failures << " UI SFX checks failed\n";
    return 1;
  }
  std::cout << "UI SFX checks passed\n";
  return 0;
}
