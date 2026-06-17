/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace platform::camera {

struct CameraInfo {
  std::string name;
  int width{320};
  int height{240};
};

class PreviewSession {
 public:
  PreviewSession() = default;
  ~PreviewSession();

  PreviewSession(const PreviewSession&)            = delete;
  PreviewSession& operator=(const PreviewSession&) = delete;

  bool start(const CameraInfo& camera, std::string& error_message);
  void stop();
  bool running() const;
  bool copy_frame_rgb565(std::vector<uint16_t>& frame, int& width, int& height) const;

 private:
  void* native_{nullptr};
};

bool find_mipi_csi_camera(CameraInfo& camera, std::string& error_message);

}  // namespace platform::camera
