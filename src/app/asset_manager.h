/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "lvgl.h"

namespace app {

class AssetManager {
 public:
  explicit AssetManager(std::string_view app_name = default_app_name(),
                        std::filesystem::path cmake_assets_root = default_cmake_assets_root());
  ~AssetManager();

  AssetManager(const AssetManager&) = delete;
  AssetManager& operator=(const AssetManager&) = delete;

  static std::string default_app_name();
  static std::filesystem::path default_cmake_assets_root();

  const std::vector<std::filesystem::path>& roots() const;
  std::filesystem::path resolve(const std::filesystem::path& relative_path) const;
  std::filesystem::path resolve_font(const std::filesystem::path& file_name) const;

  lv_font_t* load_font(const std::filesystem::path& file_name, uint32_t size);

 protected:
  struct LoadedFont;
  void add_root_if_valid_(std::filesystem::path path);

 private:
  std::vector<std::filesystem::path> roots_;
  std::vector<std::unique_ptr<LoadedFont>> loaded_fonts_;
};

}  // namespace app
