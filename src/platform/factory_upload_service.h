/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace platform::factory_upload {

enum class UploadState {
  UNAVAILABLE,
  SEARCHING,
  WAITING_HANDSHAKE,
  READY,
  QUEUED,
  SENDING,
  WAITING_RESULT_ACK,
  SUCCEEDED,
  FAILED,
};

struct UploadSnapshot {
  UploadState state{UploadState::UNAVAILABLE};
  std::string message{};
  std::string port{};
  std::uint64_t revision{0};
  bool listener_online{false};
  std::uint64_t listener_heartbeat_age_ms{0};
};

struct FactoryUploadConfig {
  std::uint16_t vendor_id{0x303A};
  std::uint16_t product_id{0x1001};
  int baud_rate{5000000};
  std::string tester_model{"FactoryTestApp"};
  std::string tester_firmware{"unknown"};
};

class FactoryUploadService {
 public:
  explicit FactoryUploadService(FactoryUploadConfig config = {});
  ~FactoryUploadService();

  FactoryUploadService(const FactoryUploadService&)            = delete;
  FactoryUploadService& operator=(const FactoryUploadService&) = delete;

  void start();
  void stop();
  bool queue_result(std::string payload, std::string& error_message);
  UploadSnapshot snapshot() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace platform::factory_upload
