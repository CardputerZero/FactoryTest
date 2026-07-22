/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cap_lora_1262_test_model.h"

#include <chrono>
#include <cstdio>
#include <utility>

#include "logger.h"

namespace model {
namespace {

constexpr std::size_t K_LORA_ITEM = 0;
constexpr std::size_t K_GPS_ITEM  = 1;

std::string gps_detail(const platform::cap_lora_1262::GpsTestResult& result) {
  std::string detail;
  if (!result.ic_info.empty()) {
    detail = result.ic_info;
  }
  if (result.nmea_detected) {
    if (!detail.empty()) {
      detail += " | ";
    }
    detail += "NMEA " + result.last_sentence_type + " (" +
              std::to_string(result.valid_sentence_count) + ")";
  }
  return detail.empty()
             ? (result.error_message.empty() ? "Waiting for IC/NMEA" : result.error_message)
             : detail;
}

}  // namespace

CapLora1262TestModel::CapLora1262TestModel() {
  snapshot_.items = {{{"SX1262", CapLora1262ItemState::PENDING, "Waiting"},
                      {"GPS", CapLora1262ItemState::PENDING, "Waiting"}}};
}

CapLora1262TestModel::~CapLora1262TestModel() { cancel(); }

void CapLora1262TestModel::start() {
  cancel();
  cancel_requested_.store(false);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.state    = CapLora1262RunState::RUNNING_LORA;
    snapshot_.headline = "Testing SX1262 communication";
    snapshot_.error_message.clear();
    snapshot_.items[K_LORA_ITEM].state  = CapLora1262ItemState::RUNNING;
    snapshot_.items[K_LORA_ITEM].detail = "Opening /dev/spidev0.1";
    snapshot_.items[K_GPS_ITEM].state   = CapLora1262ItemState::PENDING;
    snapshot_.items[K_GPS_ITEM].detail  = "Waiting";
    ++snapshot_.revision;
  }
  worker_ = std::thread(&CapLora1262TestModel::run_, this);
}

void CapLora1262TestModel::cancel() {
  cancel_requested_.store(true);
  if (worker_.joinable()) {
    worker_.join();
  }
}

CapLora1262Snapshot CapLora1262TestModel::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshot_;
}

void CapLora1262TestModel::run_() {
  platform::cap_lora_1262::RadioConfig radio_config;
  const auto radio = platform::cap_lora_1262::run_radio_test(
      radio_config,
      cancel_requested_,
      [this](const std::string& detail) {
        publish_([&](auto& snapshot) { snapshot.items[K_LORA_ITEM].detail = detail; });
      });

  const bool radio_ok = radio.detected;
  publish_([&](auto& snapshot) {
    snapshot.items[K_LORA_ITEM].state =
        radio_ok ? CapLora1262ItemState::PASSED : CapLora1262ItemState::FAILED;
    if (radio_ok) {
      char detail[96];
      std::snprintf(detail,
                    sizeof(detail),
                    "GetStatus 0x%02X | %s",
                    radio.status,
                    radio.spi_path.c_str());
      snapshot.items[K_LORA_ITEM].detail = detail;
    } else {
      snapshot.items[K_LORA_ITEM].detail =
          radio.error_message.empty() ? "LoRa test failed" : radio.error_message;
    }
    snapshot.state                    = CapLora1262RunState::RUNNING_GPS;
    snapshot.headline                 = "Reading GPS IC information";
    snapshot.items[K_GPS_ITEM].state  = CapLora1262ItemState::RUNNING;
    snapshot.items[K_GPS_ITEM].detail = "Listening on /dev/ttyS0";
  });

  if (cancel_requested_.load()) {
    publish_([](auto& snapshot) {
      snapshot.state    = CapLora1262RunState::CANCELED;
      snapshot.headline = "Test canceled";
    });
    return;
  }

  const auto gps = platform::cap_lora_1262::run_gps_test(
      "/dev/ttyS0",
      115200,
      std::chrono::seconds(10),
      cancel_requested_,
      [this](const platform::cap_lora_1262::GpsTestResult& current) {
        publish_([&](auto& snapshot) { snapshot.items[K_GPS_ITEM].detail = gps_detail(current); });
      });

  if (cancel_requested_.load()) {
    publish_([](auto& snapshot) {
      snapshot.state    = CapLora1262RunState::CANCELED;
      snapshot.headline = "Test canceled";
    });
    return;
  }

  const bool gps_ok = !gps.ic_info.empty() && gps.nmea_detected;
  publish_([&](auto& snapshot) {
    snapshot.items[K_GPS_ITEM].state =
        gps_ok ? CapLora1262ItemState::PASSED : CapLora1262ItemState::FAILED;
    snapshot.items[K_GPS_ITEM].detail = gps_detail(gps);
    snapshot.state = radio_ok && gps_ok ? CapLora1262RunState::PASSED : CapLora1262RunState::FAILED;
    snapshot.headline = snapshot.state == CapLora1262RunState::PASSED ? "CAP LoRa-1262 test passed"
                                                                      : "CAP LoRa-1262 test failed";
    if (!radio_ok) {
      snapshot.error_message = radio.error_message;
    }
    if (!gps_ok) {
      if (!snapshot.error_message.empty()) {
        snapshot.error_message += " | ";
      }
      snapshot.error_message += gps.error_message;
    }
  });
  LOG_INFO("[CAP-LORA-1262] complete radio={} gps={} radio_detail='{}' gps_detail='{}'",
           radio_ok ? "PASS" : "FAIL",
           gps_ok ? "PASS" : "FAIL",
           radio_ok ? "GetStatus OK" : radio.error_message,
           gps_detail(gps));
}

void CapLora1262TestModel::publish_(const std::function<void(CapLora1262Snapshot&)>& update) {
  std::lock_guard<std::mutex> lock(mutex_);
  update(snapshot_);
  ++snapshot_.revision;
}

const char* cap_lora_1262_item_state_text(CapLora1262ItemState state) {
  switch (state) {
    case CapLora1262ItemState::RUNNING:
      return "RUN";
    case CapLora1262ItemState::PASSED:
      return "PASS";
    case CapLora1262ItemState::FAILED:
      return "FAIL";
    case CapLora1262ItemState::PENDING:
    default:
      return "WAIT";
  }
}

}  // namespace model
