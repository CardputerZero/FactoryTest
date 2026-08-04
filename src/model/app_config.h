/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace model {

struct AppConfig {
  static constexpr int K_SCHEMA_VERSION = 1;

  struct Ui {
    bool dark_mode{true};
    std::string language{"en"};
    bool key_click_enabled{true};
    int key_click_volume_percent{50};
  } ui;

  struct Network {
    std::string iperf_host{"192.168.10.187"};
    int iperf_port{5201};
  } network;

  struct Uart {
    int baud_rate{9600};
  } uart;

  struct Logging {
    std::string level{"debug"};
    std::size_t max_segment_size_bytes{5U * 1024U * 1024U};
    std::size_t max_directory_size_bytes{50U * 1024U * 1024U};
  } logging;

  struct Factory {
    std::string station_id{"AUTO_TEST"};
  } factory;

  int schema_version{K_SCHEMA_VERSION};
};

class AppConfigStore {
 public:
  AppConfigStore();

  bool load();
  bool save();

  const AppConfig& config() const;
  AppConfig& config();
  const std::filesystem::path& path() const;
  const std::string& last_error() const;
  const std::vector<std::string>& diagnostics() const;

 private:
  std::filesystem::path path_{};
  AppConfig config_{};
  std::string last_error_{};
  std::vector<std::string> diagnostics_{};
};

}  // namespace model
