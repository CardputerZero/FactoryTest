/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cap_lora_1262_service.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#include "logger.h"
#include "spi_device.h"
#include "uart_service.h"

namespace platform::cap_lora_1262 {
namespace {

constexpr uint8_t K_GET_STATUS       = 0xC0;
constexpr const char* K_QUERY_GPS_IC = "$PCAS06,6*1D\r\n";

std::string hex_byte(uint8_t value) {
  std::ostringstream stream;
  stream << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<unsigned int>(value);
  return stream.str();
}

std::string hex_bytes(const std::vector<uint8_t>& bytes) {
  std::ostringstream stream;
  stream << std::uppercase << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (i != 0) {
      stream << ' ';
    }
    stream << std::setw(2) << static_cast<unsigned int>(bytes[i]);
  }
  return stream.str();
}

std::string escaped_text(const std::string& text, std::size_t limit = 512) {
  std::string output;
  const auto count = std::min(text.size(), limit);
  output.reserve(count + 16);
  for (std::size_t i = 0; i < count; ++i) {
    if (text[i] == '\r') {
      output += "\\r";
    } else if (text[i] == '\n') {
      output += "\\n";
    } else {
      output.push_back(text[i]);
    }
  }
  if (text.size() > limit) {
    output += "...(truncated)";
  }
  return output;
}

std::string trim(std::string value) {
  const auto is_space = [](unsigned char character) { return std::isspace(character) != 0; };
  value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), is_space));
  value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(), value.end());
  return value;
}

std::string uppercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
    return static_cast<char>(std::toupper(character));
  });
  return value;
}

int hex_nibble(char character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
  return character >= 'A' && character <= 'F' ? character - 'A' + 10 : -1;
}

bool inspect_navigation_nmea_line(const std::string& line, GpsTestResult& result) {
  if (line.size() < 7 || line.front() != '$') {
    return false;
  }
  const auto comma = line.find(',');
  const auto star  = line.find('*');
  if (comma == std::string::npos || star == std::string::npos || comma < 3 ||
      star + 2 >= line.size()) {
    return false;
  }

  const int checksum_high = hex_nibble(line[star + 1]);
  const int checksum_low  = hex_nibble(line[star + 2]);
  if (checksum_high < 0 || checksum_low < 0) {
    return false;
  }
  uint8_t checksum = 0;
  for (std::size_t index = 1; index < star; ++index) {
    checksum ^= static_cast<uint8_t>(line[index]);
  }
  if (checksum != static_cast<uint8_t>((checksum_high << 4) | checksum_low)) {
    return false;
  }

  const auto sentence_id = line.substr(1, comma - 1);
  if (sentence_id.size() < 3) {
    return false;
  }
  const auto type = sentence_id.substr(sentence_id.size() - 3);
  if (type != "GGA" && type != "GLL" && type != "GSA" && type != "GSV" && type != "RMC" &&
      type != "VTG" && type != "ZDA" && type != "GST") {
    return false;
  }

  result.nmea_detected = true;
  ++result.valid_sentence_count;
  result.last_sentence_type = type;
  LOG_DEBUG("[CAP-LORA-1262][UART][NMEA] valid type={} count={} sentence={}",
            type,
            result.valid_sentence_count,
            escaped_text(line));
  return true;
}

bool ensure_hat_power_enabled(const HatPowerConfig& config, std::string& error) {
  error.clear();
  if (!config.configured) {
    LOG_INFO("[CAP-LORA-1262][POWER] EXT 5V control disabled by configuration");
    return true;
  }
  if (config.brightness_path.empty()) {
    error = "EXT 5V brightness path is empty";
    return false;
  }

  LOG_INFO("[CAP-LORA-1262][POWER] ensuring EXT 5V enabled path={} write=1",
           config.brightness_path);
  errno = 0;
  std::ofstream output(config.brightness_path);
  if (!output) {
    error = "open " + config.brightness_path + ": " + std::strerror(errno);
    return false;
  }
  output << 1 << '\n';
  output.close();
  if (!output) {
    error = "write " + config.brightness_path + ": " + std::strerror(errno);
    return false;
  }

  int actual = 0;
  for (int attempt = 1; attempt <= 6; ++attempt) {
    std::ifstream input(config.brightness_path);
    input >> actual;
    if (input && actual != 0) {
      LOG_INFO("[CAP-LORA-1262][POWER] EXT 5V enabled readback={} settle={}ms",
               actual,
               config.settle_time_ms);
      std::this_thread::sleep_for(std::chrono::milliseconds(config.settle_time_ms));
      return true;
    }
    if (attempt < 6) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }

  error = "EXT 5V enable readback is zero or unavailable at " + config.brightness_path;
  return false;
}

class Sx1262StatusProbe {
 public:
  explicit Sx1262StatusProbe(RadioConfig config)
      : config_(std::move(config)) {}

  bool run(RadioTestResult& result, std::string& error) {
    if (!ensure_hat_power_enabled(config_.power, error)) {
      return false;
    }
    if (config_.reset.configured) {
      LOG_INFO("[CAP-LORA-1262][GPIO] RESET ready chip={} line={} waveform=HIGH-LOW-HIGH",
               config_.reset.line.chip_path,
               config_.reset.line.line_offset);
      reset_ = std::make_unique<platform::gpio::OutputLine>(config_.reset.line);
    }
    return probe_spi_candidates(result, error);
  }

 private:
  bool pulse_reset(std::string& error) {
    if (!config_.reset.configured) {
      LOG_INFO("[CAP-LORA-1262][GPIO] RESET not configured; using 20ms startup delay");
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      return true;
    }
    if (!reset_->set_value(true, error)) {
      return false;
    }
    LOG_INFO("[CAP-LORA-1262][GPIO] RESET=HIGH (idle)");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!reset_->set_value(false, error)) {
      return false;
    }
    LOG_INFO("[CAP-LORA-1262][GPIO] RESET=LOW (asserted)");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!reset_->set_value(true, error)) {
      return false;
    }
    LOG_INFO("[CAP-LORA-1262][GPIO] RESET=HIGH (released)");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return true;
  }

  bool wait_ready(std::string& error) const {
    if (!config_.busy.configured) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      return true;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
      bool value = false;
      if (!platform::gpio::get_input_value(config_.busy.line, value, error)) {
        return false;
      }
      const bool busy = config_.busy.active_high ? value : !value;
      if (!busy) {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    error = "SX1262 BUSY timeout";
    return false;
  }

  bool probe_spi_candidates(RadioTestResult& result, std::string& error) {
    std::vector<std::string> candidates;
    const auto add_candidate = [&](const std::string& path) {
      if (!path.empty() &&
          std::find(candidates.begin(), candidates.end(), path) == candidates.end()) {
        candidates.push_back(path);
      }
    };
    add_candidate(config_.spi_path);
    add_candidate("/dev/spidev0.1");
    add_candidate("/dev/spidev0.0");

    std::string last_error;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
      const auto& path = candidates[index];
      LOG_INFO("[CAP-LORA-1262][SPI] probe={}/{} path={} mode=0 speed={}Hz bits=8",
               index + 1,
               candidates.size(),
               path,
               config_.spi_speed_hz);

      platform::spi::DeviceConfig spi_config;
      spi_config.path          = path;
      spi_config.speed_hz      = config_.spi_speed_hz;
      spi_config.mode          = 0;
      spi_config.bits_per_word = 8;
      std::string candidate_error;
      auto spi = platform::spi::Device::open(spi_config, candidate_error);
      if (!spi) {
        LOG_WARN("[CAP-LORA-1262][SPI] open failed path={}: {}", path, candidate_error);
        last_error = std::move(candidate_error);
        continue;
      }
      if (!pulse_reset(candidate_error)) {
        error = "RESET failed while probing " + path + ": " + candidate_error;
        return false;
      }
      if (!wait_ready(candidate_error)) {
        LOG_WARN("[CAP-LORA-1262][SPI] BUSY wait failed path={}: {}", path, candidate_error);
        last_error = std::move(candidate_error);
        continue;
      }

      const std::vector<uint8_t> tx{K_GET_STATUS, 0x00};
      std::vector<uint8_t> rx;
      if (!spi->transfer(tx, rx, candidate_error)) {
        LOG_WARN("[CAP-LORA-1262][SPI] GetStatus failed path={}: {}", path, candidate_error);
        last_error = std::move(candidate_error);
        continue;
      }
      const uint8_t status = rx.size() >= 2 ? rx[1] : 0x00;
      LOG_INFO("[CAP-LORA-1262][SPI] GetStatus path={} TX={} RX={} status=0x{}",
               path,
               hex_bytes(tx),
               hex_bytes(rx),
               hex_byte(status));
      if (status == 0x00 || status == 0xFF) {
        last_error = "invalid SX1262 status 0x" + hex_byte(status) + " on " + path;
        continue;
      }

      result.detected = true;
      result.status   = status;
      result.spi_path = path;
      LOG_INFO("[CAP-LORA-1262][SPI] SX1262 detected path={} status=0x{}", path, hex_byte(status));
      return true;
    }

    error = "no valid SX1262 GetStatus response";
    if (!last_error.empty()) {
      error += ": " + last_error;
    }
    return false;
  }

  RadioConfig config_{};
  std::unique_ptr<platform::gpio::OutputLine> reset_{};
};

bool inspect_uart_ic_line(const std::string& raw_line, GpsTestResult& result) {
  const auto line = trim(raw_line);
  if (line.empty()) {
    return false;
  }
  const auto normalized = uppercase(line);
  if (normalized.find("IC=") != std::string::npos || normalized.find("IC:") != std::string::npos) {
    result.ic_info = line;
    LOG_INFO("[CAP-LORA-1262][UART] IC information={}", escaped_text(line));
    return true;
  }
  return false;
}

}  // namespace

RadioTestResult run_radio_test(const RadioConfig& config,
                               const std::atomic_bool& cancel_requested,
                               const ProgressCallback& progress) {
  RadioTestResult result;
  if (cancel_requested.load()) {
    result.error_message = "SX1262 test canceled";
    return result;
  }
  if (progress) {
    progress("Reading SX1262 status at 1 MHz");
  }

  LOG_INFO("[CAP-LORA-1262][CONFIG] SPI preferred_path={} speed={}Hz command=GetStatus",
           config.spi_path,
           config.spi_speed_hz);
  Sx1262StatusProbe probe(config);
  if (!probe.run(result, result.error_message)) {
    LOG_ERROR("[CAP-LORA-1262][SPI] status test failed: {}", result.error_message);
    return result;
  }
  if (progress) {
    progress("Status 0x" + hex_byte(result.status) + " on " + result.spi_path);
  }
  return result;
}

GpsTestResult run_gps_test(const std::string& uart_path,
                           int baud_rate,
                           std::chrono::milliseconds timeout,
                           const std::atomic_bool& cancel_requested,
                           const std::function<void(const GpsTestResult&)>& progress) {
  GpsTestResult result;
  platform::connectivity::UartOpenResult open_result;
  auto uart = platform::connectivity::UartDebugSession::open(uart_path, baud_rate, open_result);
  if (!uart) {
    result.error_message = open_result.message;
    LOG_ERROR("[CAP-LORA-1262][UART] open failed path={} baud={}: {}",
              uart_path,
              baud_rate,
              result.error_message);
    return result;
  }

  LOG_INFO(
      "[CAP-LORA-1262][UART] reading IC information and navigation NMEA path={} baud={} "
      "timeout={}ms",
      uart_path,
      baud_rate,
      timeout.count());
  std::string write_error;
  LOG_INFO("[CAP-LORA-1262][UART] TX query={}", escaped_text(K_QUERY_GPS_IC));
  if (!uart->write_text(K_QUERY_GPS_IC, write_error)) {
    result.error_message = "GPS IC query failed: " + write_error;
    LOG_ERROR("[CAP-LORA-1262][UART] {}", result.error_message);
    return result;
  }

  std::string pending;
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline && !cancel_requested.load()) {
    std::string read_error;
    const auto chunk = uart->read_available(read_error);
    if (!read_error.empty()) {
      result.error_message = std::move(read_error);
      LOG_ERROR("[CAP-LORA-1262][UART] read failed: {}", result.error_message);
      return result;
    }
    if (!chunk.empty()) {
      LOG_DEBUG("[CAP-LORA-1262][UART] RX len={} text={}", chunk.size(), escaped_text(chunk));
      pending += chunk;
    }

    std::size_t newline = 0;
    while ((newline = pending.find_first_of("\r\n")) != std::string::npos) {
      const auto line = pending.substr(0, newline);
      pending.erase(0, newline + 1);
      while (!pending.empty() && (pending.front() == '\r' || pending.front() == '\n')) {
        pending.erase(pending.begin());
      }
      const auto previous_ic    = result.ic_info;
      const auto previous_count = result.valid_sentence_count;
      inspect_uart_ic_line(line, result);
      inspect_navigation_nmea_line(trim(line), result);
      if (progress &&
          (result.ic_info != previous_ic || result.valid_sentence_count != previous_count)) {
        progress(result);
      }
    }
    if (!result.ic_info.empty() && result.nmea_detected) {
      break;
    }
    if (pending.size() > 2048) {
      pending.erase(0, pending.size() - 1024);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }

  if (cancel_requested.load()) {
    result.error_message = "UART test canceled";
  } else if (result.ic_info.empty()) {
    result.error_message = "No GPS IC information received from " + uart_path;
  } else if (!result.nmea_detected) {
    result.error_message = "No valid navigation NMEA received from " + uart_path;
  }
  if (!result.ic_info.empty() && result.nmea_detected) {
    LOG_INFO("[CAP-LORA-1262][UART] complete IC='{}' NMEA_count={} last_type={}",
             escaped_text(result.ic_info),
             result.valid_sentence_count,
             result.last_sentence_type);
  } else {
    LOG_ERROR("[CAP-LORA-1262][UART] failed: {}", result.error_message);
  }
  return result;
}

}  // namespace platform::cap_lora_1262
