/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cap_cc1101_test_model.h"

#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>

#include "gpio_service.h"
#include "logger.h"

namespace model {
namespace {

constexpr const char* K_EXT_5V_PATH = "/sys/class/leds/ext_5v_out/brightness";
constexpr unsigned int K_EXT_5V_SETTLE_MS = 300;

constexpr std::size_t K_CC_CHIP_ITEM = 0;
constexpr std::size_t K_ST_CHIP_ITEM = 1;

std::string hex_byte(uint8_t value) {
  std::ostringstream stream;
  stream << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<unsigned int>(value);
  return stream.str();
}

}  // namespace

CapCc1101TestModel::CapCc1101TestModel() {
  snapshot_.items = {{{"CC1101", CapCc1101ItemState::PENDING, "Waiting"},
                      {"ST25R", CapCc1101ItemState::PENDING, "Waiting"}}};
}

CapCc1101TestModel::~CapCc1101TestModel() { cancel(); }

void CapCc1101TestModel::start() {
  cancel();
  cancel_requested_.store(false);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.state                 = CapCc1101RunState::RUNNING;
    snapshot_.headline              = "Testing CC1101 over SPI";
    snapshot_.error_message         = {};
    snapshot_.items[K_CC_CHIP_ITEM] = {"CC1101",
                                       CapCc1101ItemState::RUNNING,
                                       "Opening /dev/spidev0.1"};
    snapshot_.items[K_ST_CHIP_ITEM] = {"ST25R", CapCc1101ItemState::PENDING, "Waiting"};
    ++snapshot_.revision;
  }
  worker_ = std::thread(&CapCc1101TestModel::run_, this);
}

void CapCc1101TestModel::cancel() {
  cancel_requested_.store(true);
  if (worker_.joinable()) {
    worker_.join();
  }
}

CapCc1101Snapshot CapCc1101TestModel::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshot_;
}

void CapCc1101TestModel::run_() {
  std::string power_error;
  if (!platform::gpio::write_sysfs_binary_value(K_EXT_5V_PATH, true, power_error)) {
    std::string disable_error;
    if (!platform::gpio::write_sysfs_binary_value(K_EXT_5V_PATH, false, disable_error)) {
      LOG_ERROR("[CAP-CC1101][POWER] failed to roll back EXT 5V after enable failure: {}",
                disable_error);
    }
    publish_([&](auto& snapshot) {
      snapshot.state = CapCc1101RunState::FAILED;
      snapshot.headline = "CAP-CC1101 test failed";
      snapshot.error_message = "EXT 5V enable failed: " + power_error;
      snapshot.items[K_CC_CHIP_ITEM].state = CapCc1101ItemState::FAILED;
      snapshot.items[K_CC_CHIP_ITEM].detail = snapshot.error_message;
    });
    return;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(K_EXT_5V_SETTLE_MS));
  struct PowerCleanup {
    ~PowerCleanup() {
      std::string error;
      if (!platform::gpio::write_sysfs_binary_value(K_EXT_5V_PATH, false, error)) {
        LOG_ERROR("[CAP-CC1101][POWER] failed to disable EXT 5V: {}", error);
      }
    }
  } power_cleanup;
  const auto cc = platform::cap_cc1101::run_cc1101_test(
      {},
      cancel_requested_,
      [this](const std::string& detail) {
        publish_([&](auto& snapshot) { snapshot.items[K_CC_CHIP_ITEM].detail = detail; });
      });

  if (cancel_requested_.load()) {
    publish_([](auto& snapshot) {
      snapshot.state    = CapCc1101RunState::CANCELED;
      snapshot.headline = "Test canceled";
    });
    return;
  }

  const bool cc_ok = cc.detected && cc.initialized;
  publish_([&](auto& snapshot) {
    snapshot.items[K_CC_CHIP_ITEM].state =
        cc_ok ? CapCc1101ItemState::PASSED : CapCc1101ItemState::FAILED;
    snapshot.items[K_CC_CHIP_ITEM].detail =
        cc_ok ? "PART " + hex_byte(cc.part_number) + " VER " + hex_byte(cc.version) + " ST " +
                    hex_byte(cc.chip_status) + " MARC " + hex_byte(cc.marc_state)
              : cc.error_message;
    snapshot.headline                     = "Testing ST25R3916 over SPI";
    snapshot.items[K_ST_CHIP_ITEM].state  = CapCc1101ItemState::RUNNING;
    snapshot.items[K_ST_CHIP_ITEM].detail = "SPI bus via spidev0.1, software CS on GPIO22";
  });

  const auto st = platform::cap_cc1101::run_st25r3916_test(
      {},
      cancel_requested_,
      [this](const std::string& detail) {
        publish_([&](auto& snapshot) { snapshot.items[K_ST_CHIP_ITEM].detail = detail; });
      });

  if (cancel_requested_.load()) {
    publish_([](auto& snapshot) {
      snapshot.state    = CapCc1101RunState::CANCELED;
      snapshot.headline = "Test canceled";
    });
    return;
  }

  const bool st_ok = st.detected && st.communication_verified;
  publish_([&](auto& snapshot) {
    snapshot.items[K_ST_CHIP_ITEM].state =
        st_ok ? CapCc1101ItemState::PASSED : CapCc1101ItemState::FAILED;
    snapshot.items[K_ST_CHIP_ITEM].detail =
        st_ok ? "ID " + hex_byte(st.identity) + " OP " + hex_byte(st.operation_control) + " FIFO " +
                    hex_byte(st.fifo_status1) + "/" + hex_byte(st.fifo_status2)
              : st.error_message;
    snapshot.state    = cc_ok && st_ok ? CapCc1101RunState::PASSED : CapCc1101RunState::FAILED;
    snapshot.headline = snapshot.state == CapCc1101RunState::PASSED ? "CAP-CC1101 test passed"
                                                                    : "CAP-CC1101 test failed";
    if (!cc_ok) {
      snapshot.error_message = cc.error_message;
    }
    if (!st_ok) {
      if (!snapshot.error_message.empty()) {
        snapshot.error_message += " | ";
      }
      snapshot.error_message += st.error_message;
    }
  });
  LOG_INFO("[CAP-CC1101] complete cc={} st25r={}", cc_ok, st_ok);
}

void CapCc1101TestModel::publish_(const std::function<void(CapCc1101Snapshot&)>& update) {
  std::lock_guard<std::mutex> lock(mutex_);
  update(snapshot_);
  ++snapshot_.revision;
}

const char* cap_cc1101_item_state_text(CapCc1101ItemState state) {
  switch (state) {
    case CapCc1101ItemState::RUNNING:
      return "RUN";
    case CapCc1101ItemState::PASSED:
      return "PASS";
    case CapCc1101ItemState::FAILED:
      return "FAIL";
    case CapCc1101ItemState::PENDING:
    default:
      return "WAIT";
  }
}

}  // namespace model
