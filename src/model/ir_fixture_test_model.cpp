/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ir_fixture_test_model.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>

#include "ir_service.h"
#include "logger.h"

namespace model {
namespace {

constexpr std::size_t K_TX_ITEM           = 0;
constexpr std::size_t K_RX_ITEM           = 1;
constexpr std::size_t K_TX_PACKET_COUNT   = 4;
constexpr std::size_t K_RX_PACKET_COUNT   = 5;
constexpr uint16_t K_FIXTURE_ADDRESS      = 0xED2F;
constexpr unsigned int K_TX_INTERVAL_MS   = 250;
constexpr unsigned int K_RX_TIMEOUT_MS    = 3500;
constexpr unsigned int K_POLL_INTERVAL_MS = 20;

std::string hex_word(uint16_t value) {
  std::ostringstream stream;
  stream << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << value;
  return stream.str();
}

uint16_t random_dut_address(std::mt19937& generator) {
  std::uniform_int_distribution<unsigned int> distribution(1, 0xFFFE);
  while (true) {
    const auto address = static_cast<uint16_t>(distribution(generator));
    const auto low     = static_cast<uint8_t>(address & 0xFFU);
    const auto high    = static_cast<uint8_t>((address >> 8U) & 0xFFU);
    if (address != K_FIXTURE_ADDRESS && static_cast<uint8_t>(low ^ high) != 0xFFU) {
      return address;
    }
  }
}

std::array<uint16_t, K_TX_PACKET_COUNT> random_commands(std::mt19937& generator) {
  std::uniform_int_distribution<unsigned int> distribution(1, 0xFE);
  std::array<uint16_t, K_TX_PACKET_COUNT> commands{};
  for (auto& command : commands) {
    // The fixture returns only the low command byte. Keep that byte zero so both
    // the released fixture and the corrected echo implementation return F1..F5.
    command = static_cast<uint16_t>(distribution(generator) << 8U);
  }
  return commands;
}

uint16_t expected_fixture_command(std::size_t index) {
  return static_cast<uint16_t>(0x00F1U + index);
}

const char* tx_progress_text(std::size_t index) {
  constexpr std::array<const char*, K_TX_PACKET_COUNT> TEXT = {
      "Sending random frame 1/4",
      "Sending random frame 2/4",
      "Sending random frame 3/4",
      "Sending random frame 4/4",
  };
  return index < TEXT.size() ? TEXT[index] : "Sending random NEC32 frames";
}

const char* rx_progress_text(std::size_t count) {
  constexpr std::array<const char*, K_RX_PACKET_COUNT + 1> TEXT = {
      "Waiting for fixture reply",
      "Verified fixture reply 1/5",
      "Verified fixture reply 2/5",
      "Verified fixture reply 3/5",
      "Verified fixture reply 4/5",
      "Verified fixture reply 5/5",
  };
  return count < TEXT.size() ? TEXT[count] : TEXT.back();
}

}  // namespace

IrFixtureTestModel::IrFixtureTestModel() {
  snapshot_.items = {{{"IR TX", IrFixtureItemState::PENDING, "Waiting"},
                      {"IR RX", IrFixtureItemState::PENDING, "Waiting"}}};
}

IrFixtureTestModel::~IrFixtureTestModel() { cancel(); }

void IrFixtureTestModel::start() {
  cancel();
  cancel_requested_.store(false);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.state            = IrFixtureRunState::RUNNING;
    snapshot_.headline         = "Checking IR transmitter and receiver";
    snapshot_.error_message    = {};
    snapshot_.items[K_TX_ITEM] = {"IR TX", IrFixtureItemState::RUNNING, "Preparing random frames"};
    snapshot_.items[K_RX_ITEM] = {"IR RX", IrFixtureItemState::PENDING, "Waiting for fixture"};
    ++snapshot_.revision;
  }
  worker_ = std::thread(&IrFixtureTestModel::run_, this);
}

void IrFixtureTestModel::cancel() {
  cancel_requested_.store(true);
  if (worker_.joinable()) {
    worker_.join();
  }
}

IrFixtureSnapshot IrFixtureTestModel::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshot_;
}

void IrFixtureTestModel::run_() {
  std::random_device random_device;
  std::mt19937 generator(
      random_device() ^
      static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()));
  const uint16_t dut_address = random_dut_address(generator);
  const auto commands        = random_commands(generator);

  const auto sender_info = platform::ir::read_sender_info();
  if (!sender_info.available) {
    fail_(K_TX_ITEM,
          sender_info.error_message.empty() ? "IR transmitter unavailable"
                                            : sender_info.error_message);
    return;
  }

  const auto receiver_info = platform::ir::read_receiver_info();
  if (!receiver_info.available) {
    fail_(K_RX_ITEM,
          receiver_info.error_message.empty() ? "IR receiver unavailable"
                                              : receiver_info.error_message);
    return;
  }

  LOG_INFO("[IR-FIXTURE] half-duplex run address={} tx_count={} expected_fixture={}",
           hex_word(dut_address),
           K_TX_PACKET_COUNT,
           hex_word(K_FIXTURE_ADDRESS));

  std::chrono::steady_clock::time_point tx_completed_at{};
  for (std::size_t i = 0; i < commands.size(); ++i) {
    if (cancel_requested_.load()) {
      break;
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.headline                = "Sending random NEC32 frames";
      snapshot_.items[K_TX_ITEM].detail = tx_progress_text(i);
      ++snapshot_.revision;
    }
    const auto result = platform::ir::send_nec_packet(dut_address, commands[i]);
    if (!result.success) {
      fail_(K_TX_ITEM, result.message.empty() ? "IR frame send failed" : result.message);
      return;
    }
    tx_completed_at = std::chrono::steady_clock::now();
    LOG_INFO("[IR-FIXTURE][TX] {}/{} address={} command={}",
             i + 1,
             commands.size(),
             hex_word(dut_address),
             hex_word(commands[i]));
    if (i + 1 < commands.size() && !delay_(K_TX_INTERVAL_MS)) {
      break;
    }
  }

  if (cancel_requested_.load()) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.state    = IrFixtureRunState::CANCELED;
    snapshot_.headline = "IR fixture test canceled";
    ++snapshot_.revision;
    return;
  }

  platform::ir::IrReceiverSession receiver;
  if (!receiver.start(receiver_info, K_FIXTURE_ADDRESS, true)) {
    const auto receiver_snapshot = receiver.snapshot();
    fail_(
        K_RX_ITEM,
        receiver_snapshot.message.empty() ? "IR receiver unavailable" : receiver_snapshot.message);
    return;
  }
  const auto rx_switch_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - tx_completed_at)
                                .count();
  LOG_INFO("[IR-FIXTURE] TX complete; RX opened after {} us", rx_switch_us);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.headline                = "Validating fixture response";
    snapshot_.items[K_TX_ITEM].detail = "4 frames sent; waiting for verified reply";
    snapshot_.items[K_RX_ITEM].state  = IrFixtureItemState::RUNNING;
    snapshot_.items[K_RX_ITEM].detail = rx_progress_text(0);
    ++snapshot_.revision;
  }

  std::size_t received_count = 0;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(K_RX_TIMEOUT_MS);
  while (!cancel_requested_.load() && std::chrono::steady_clock::now() < deadline &&
         received_count < K_RX_PACKET_COUNT) {
    const auto received = receiver.poll();
    if (received.data_changed && received.address_matched) {
      const uint16_t expected = expected_fixture_command(received_count);
      LOG_INFO("[IR-FIXTURE][RX] {}/{} address={} command={} expected={}",
               received_count + 1,
               K_RX_PACKET_COUNT,
               hex_word(received.address),
               hex_word(received.command),
               hex_word(expected));
      if (received.protocol != "NEC32") {
        fail_(K_RX_ITEM,
              "Reply " + std::to_string(received_count + 1) + " expected NEC32, got " +
                  (received.protocol.empty() ? "unknown protocol" : received.protocol));
        return;
      }
      if (received.command != expected) {
        fail_(K_RX_ITEM,
              "Reply " + std::to_string(received_count + 1) + " expected " + hex_word(expected) +
                  ", got " + hex_word(received.command));
        return;
      }
      ++received_count;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.items[K_RX_ITEM].detail = rx_progress_text(received_count);
        ++snapshot_.revision;
      }
    }
    if (received_count < K_RX_PACKET_COUNT && !delay_(K_POLL_INTERVAL_MS)) {
      break;
    }
  }

  if (cancel_requested_.load()) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.state    = IrFixtureRunState::CANCELED;
    snapshot_.headline = "IR fixture test canceled";
    ++snapshot_.revision;
    return;
  }
  if (received_count != K_RX_PACKET_COUNT) {
    fail_(K_RX_ITEM, "Fixture reply timed out: " + std::to_string(received_count) + "/5 received");
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.items[K_TX_ITEM].state  = IrFixtureItemState::PASSED;
    snapshot_.items[K_TX_ITEM].detail = "Fixture acknowledged all 4 frames";
    snapshot_.items[K_RX_ITEM].state  = IrFixtureItemState::PASSED;
    snapshot_.items[K_RX_ITEM].detail = "0xED2F: F1 F2 F3 F4 F5 verified";
    snapshot_.state                   = IrFixtureRunState::PASSED;
    snapshot_.headline                = "IR fixture test passed";
    ++snapshot_.revision;
  }
  LOG_INFO("[IR-FIXTURE] PASS: fixture response address and F1..F5 order verified");
}

bool IrFixtureTestModel::delay_(unsigned int milliseconds) {
  constexpr unsigned int K_SLICE_MS = 10;
  unsigned int elapsed              = 0;
  while (elapsed < milliseconds && !cancel_requested_.load()) {
    const unsigned int slice = std::min(K_SLICE_MS, milliseconds - elapsed);
    std::this_thread::sleep_for(std::chrono::milliseconds(slice));
    elapsed += slice;
  }
  return !cancel_requested_.load();
}

void IrFixtureTestModel::fail_(std::size_t item_index, const std::string& message) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& item : snapshot_.items) {
    if (item.state == IrFixtureItemState::RUNNING) {
      item.state  = IrFixtureItemState::FAILED;
      item.detail = "Fixture acknowledgement not verified";
    }
  }
  if (item_index < snapshot_.items.size()) {
    snapshot_.items[item_index].state  = IrFixtureItemState::FAILED;
    snapshot_.items[item_index].detail = message;
  }
  snapshot_.state         = IrFixtureRunState::FAILED;
  snapshot_.headline      = "IR fixture test failed";
  snapshot_.error_message = message;
  ++snapshot_.revision;
  LOG_ERROR("[IR-FIXTURE] FAIL: {}", message);
}

const char* ir_fixture_item_state_text(IrFixtureItemState state) {
  switch (state) {
    case IrFixtureItemState::RUNNING:
      return "RUN";
    case IrFixtureItemState::PASSED:
      return "PASS";
    case IrFixtureItemState::FAILED:
      return "FAIL";
    case IrFixtureItemState::PENDING:
    default:
      return "WAIT";
  }
}

}  // namespace model
