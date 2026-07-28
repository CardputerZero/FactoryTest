/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace platform::py32_upgrade {

struct Progress {
  int percent{0};
  std::string status{};
};

using ProgressCallback = std::function<void(const Progress& progress)>;

struct Result {
  bool success{false};
  std::string previous_version{};
  std::string current_version{};
  std::string error_status{"Upgrade failed"};
  std::string error_message{};
};

Result run(const std::filesystem::path& archive_path,
           const ProgressCallback& progress_callback = {});

}  // namespace platform::py32_upgrade
