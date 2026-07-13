/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ir_service.h"

#if defined(FACTORY_TEST_SCONS_BUILD)
#include "factory_test_config.h"
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "logger.h"

#ifndef APP_USE_LIRC
#define APP_USE_LIRC 0
#endif

#if APP_USE_LIRC
#include <fcntl.h>
#include <linux/lirc.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace platform::ir {
namespace {

constexpr uint32_t K_CARRIER_HZ       = 38000;
constexpr uint32_t K_DUTY_CYCLE       = 50;
constexpr uint32_t K_REC_TIMEOUT_US   = 30000;
constexpr std::size_t K_PACKET_BYTES  = 4;
constexpr uint32_t K_NEC_HDR_PULSE_US = 9000;
constexpr uint32_t K_NEC_HDR_SPACE_US = 4500;
constexpr uint32_t K_NEC_BIT_PULSE_US = 560;
constexpr uint32_t K_NEC_ZERO_SPACE_US = 560;
constexpr uint32_t K_NEC_ONE_SPACE_US = 1690;
constexpr const char* K_PROTOCOL_NEC   = "NEC";
constexpr const char* K_PROTOCOL_NECX  = "NECX";
constexpr const char* K_PROTOCOL_NEC32 = "NEC32";

std::string trim(std::string value) {
  while (!value.empty() && (value.back() == '\n' || value.back() == '\r' ||
                            value.back() == ' ' || value.back() == '\t')) {
    value.pop_back();
  }
  const auto first = value.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return {};
  }
  if (first > 0) {
    value.erase(0, first);
  }
  return value;
}

std::string read_first_line(const std::filesystem::path& path) {
  std::ifstream stream(path);
  std::string line;
  if (std::getline(stream, line)) {
    return trim(std::move(line));
  }
  return {};
}

std::string read_uevent_value(const std::filesystem::path& path, const std::string& key) {
  std::ifstream stream(path);
  std::string line;
  const std::string prefix = key + "=";
  while (std::getline(stream, line)) {
    if (line.rfind(prefix, 0) == 0) {
      return trim(line.substr(prefix.size()));
    }
  }
  return {};
}

std::vector<std::string> list_rc_names() {
  std::vector<std::string> rc_names;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator("/sys/class/rc", error)) {
    const auto name = entry.path().filename().string();
    if (name.rfind("rc", 0) == 0) {
      rc_names.push_back(name);
    }
  }
  std::sort(rc_names.begin(), rc_names.end());
  return rc_names;
}

std::vector<std::string> find_lirc_paths_for_rc(const std::string& rc_name) {
  std::vector<std::string> lirc_paths;
  const std::filesystem::path rc_path = std::filesystem::path("/sys/class/rc") / rc_name;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(rc_path, error)) {
    const auto name = entry.path().filename().string();
    if (name.rfind("lirc", 0) == 0) {
      lirc_paths.push_back("/dev/" + name);
    }
  }
  std::sort(lirc_paths.begin(), lirc_paths.end());
  return lirc_paths;
}

#if APP_USE_LIRC
int open_lirc_for_features(const std::string& lirc_path) {
  for (int flags : {O_RDWR | O_NONBLOCK, O_RDONLY | O_NONBLOCK, O_WRONLY | O_NONBLOCK}) {
    const int fd = open(lirc_path.c_str(), flags);
    if (fd >= 0) {
      return fd;
    }
  }
  return -1;
}

bool supports_sender(uint32_t features) {
  return (features & (LIRC_CAN_SEND_PULSE | LIRC_CAN_SEND_RAW)) != 0;
}

bool supports_receiver(uint32_t features) {
  return (features & (LIRC_CAN_REC_MODE2 | LIRC_CAN_REC_RAW | LIRC_CAN_REC_SCANCODE)) != 0;
}
#endif

IrDeviceInfo make_device_info(const std::string& rc_name,
                              const std::string& lirc_path,
                              bool sender) {
  IrDeviceInfo info;
  info.rc_name   = rc_name;
  info.lirc_path = lirc_path;

  const std::filesystem::path rc_path = std::filesystem::path("/sys/class/rc") / rc_name;
  info.driver_name = read_uevent_value(rc_path / "uevent", "DRV_NAME");
  info.device_name = read_uevent_value(rc_path / "uevent", "DEV_NAME");
  if (info.driver_name.empty()) {
    info.driver_name = read_uevent_value(rc_path / "device" / "uevent", "DRIVER");
  }
  if (info.device_name.empty()) {
    info.device_name = read_first_line(rc_path / "name");
  }

  if (info.lirc_path.empty()) {
    info.error_message = std::string("No LIRC node found for ") + rc_name;
    LOG_DEBUG("IR {} device unavailable: {}", sender ? "sender" : "receiver", info.error_message);
    return info;
  }

#if APP_USE_LIRC
  LOG_VERBOSE("probing IR {} device: rc={} path={}",
              sender ? "sender" : "receiver",
              rc_name,
              info.lirc_path);
  const int fd = open_lirc_for_features(info.lirc_path);
  if (fd < 0) {
    info.error_message = std::string("Open failed: ") + std::strerror(errno);
    LOG_WARN("IR {} open failed: rc={} path={} error={}",
             sender ? "sender" : "receiver",
             rc_name,
             info.lirc_path,
             info.error_message);
    return info;
  }

  uint32_t features = 0;
  if (ioctl(fd, LIRC_GET_FEATURES, &features) == 0) {
    info.features = features;
  } else {
    info.error_message = std::string("Read LIRC features failed: ") + std::strerror(errno);
    LOG_WARN("IR {} feature probe failed: rc={} path={} error={}",
             sender ? "sender" : "receiver",
             rc_name,
             info.lirc_path,
             info.error_message);
    close(fd);
    return info;
  }
  close(fd);

  const bool supported = sender ? supports_sender(info.features) : supports_receiver(info.features);
  if (!supported) {
    info.error_message = "LIRC device does not advertise required capability";
    LOG_WARN("IR {} missing capability: rc={} path={} features=0x{:08x}",
             sender ? "sender" : "receiver",
             rc_name,
             info.lirc_path,
             info.features);
    return info;
  }

  info.available = true;
  LOG_DEBUG("IR {} device ready: rc={} path={} driver='{}' name='{}' features=0x{:08x}",
            sender ? "sender" : "receiver",
            rc_name,
            info.lirc_path,
            info.driver_name,
            info.device_name,
            info.features);
#else
  info.error_message = "liblirc-dev support was not found at build time";
  LOG_DEBUG("IR {} backend disabled: {}", sender ? "sender" : "receiver", info.error_message);
#endif
  return info;
}

IrDeviceInfo find_ir_device(bool sender) {
  IrDeviceInfo first_probe_error;
  bool has_probe_error  = false;
  std::size_t candidates = 0;

  for (const auto& rc_name : list_rc_names()) {
    for (const auto& lirc_path : find_lirc_paths_for_rc(rc_name)) {
      ++candidates;
      auto info = make_device_info(rc_name, lirc_path, sender);
      if (info.available) {
        return info;
      }
      if (!has_probe_error && info.features == 0) {
        first_probe_error = info;
        has_probe_error  = true;
      }
    }
  }

  if (has_probe_error) {
    return first_probe_error;
  }

  IrDeviceInfo info;
  info.error_message =
      candidates == 0 ? "No LIRC nodes found under /sys/class/rc"
                      : std::string("No LIRC device advertises IR ") +
                            (sender ? "sender" : "receiver") + " capability";
  LOG_WARN("IR {} discovery failed: {}", sender ? "sender" : "receiver", info.error_message);
  return info;
}

std::string errno_message(const char* prefix) {
  return std::string(prefix) + ": " + std::strerror(errno);
}

bool close_fd(int& fd) {
#if APP_USE_LIRC
  if (fd >= 0) {
    close(fd);
    fd = -1;
    return true;
  }
#else
  (void)fd;
#endif
  return false;
}

uint16_t random_command() {
  std::random_device rd;
  std::mt19937 generator(rd() ^ static_cast<unsigned int>(
                                    std::chrono::steady_clock::now().time_since_epoch().count()));
  std::uniform_int_distribution<int> dist(0, 0xffff);
  return static_cast<uint16_t>(dist(generator));
}

uint16_t make_nec_command(uint8_t command_byte) {
  return static_cast<uint16_t>(command_byte) |
         static_cast<uint16_t>(static_cast<uint8_t>(~command_byte) << 8U);
}

std::vector<uint8_t> make_nec_bytes(uint16_t address, uint16_t command) {
  return {static_cast<uint8_t>(address & 0xffU),
          static_cast<uint8_t>((address >> 8U) & 0xffU),
          static_cast<uint8_t>(command & 0xffU),
          static_cast<uint8_t>((command >> 8U) & 0xffU)};
}

std::vector<uint8_t> make_standard_nec_bytes(uint8_t address, uint8_t command) {
  return {address,
          static_cast<uint8_t>(~address),
          command,
          static_cast<uint8_t>(~command)};
}

bool parse_nec_bytes(const std::vector<uint8_t>& bytes,
                     uint16_t& address,
                     uint16_t& command,
                     std::string& protocol) {
  if (bytes.size() != K_PACKET_BYTES) {
    return false;
  }

  address = static_cast<uint16_t>(bytes[0]) | static_cast<uint16_t>(bytes[1] << 8U);
  command = static_cast<uint16_t>(bytes[2]) | static_cast<uint16_t>(bytes[3] << 8U);
  if (static_cast<uint8_t>(bytes[0] ^ bytes[1]) == 0xffU) {
    protocol = K_PROTOCOL_NEC;
    return true;
  }
  protocol = static_cast<uint8_t>(bytes[2] ^ bytes[3]) == 0xffU ? K_PROTOCOL_NECX
                                                                : K_PROTOCOL_NEC32;
  return true;
}

std::vector<uint32_t> encode_nec_packet(const std::vector<uint8_t>& bytes) {
  std::vector<uint32_t> pulses;
  pulses.reserve(2 + bytes.size() * 16 + 1);
  pulses.push_back(K_NEC_HDR_PULSE_US);
  pulses.push_back(K_NEC_HDR_SPACE_US);
  for (uint8_t byte : bytes) {
    for (int bit = 0; bit < 8; ++bit) {
      pulses.push_back(K_NEC_BIT_PULSE_US);
      pulses.push_back((byte & (1U << bit)) ? K_NEC_ONE_SPACE_US : K_NEC_ZERO_SPACE_US);
    }
  }
  pulses.push_back(K_NEC_BIT_PULSE_US);
  return pulses;
}

bool approx(uint32_t value, uint32_t target, uint32_t tolerance) {
  return value + tolerance >= target && value <= target + tolerance;
}

bool decode_nec_packet_from(const std::vector<uint32_t>& durations,
                            std::size_t start,
                            std::vector<uint8_t>& output) {
  constexpr std::size_t required = 2 + K_PACKET_BYTES * 16 + 1;
  if (durations.size() < start + required) {
    return false;
  }
  if (!approx(durations[start], K_NEC_HDR_PULSE_US, 2500) ||
      !approx(durations[start + 1], K_NEC_HDR_SPACE_US, 1800)) {
    return false;
  }

  std::vector<uint8_t> bytes;
  bytes.reserve(K_PACKET_BYTES);
  std::size_t index = start + 2;
  for (std::size_t byte_index = 0; byte_index < K_PACKET_BYTES; ++byte_index) {
    uint8_t byte = 0;
    for (int bit = 0; bit < 8; ++bit) {
      const uint32_t pulse = durations[index++];
      const uint32_t space = durations[index++];
      if (!approx(pulse, K_NEC_BIT_PULSE_US, 450)) {
        return false;
      }
      if (approx(space, K_NEC_ONE_SPACE_US, 850)) {
        byte |= static_cast<uint8_t>(1U << bit);
      } else if (!approx(space, K_NEC_ZERO_SPACE_US, 500)) {
        return false;
      }
    }
    bytes.push_back(byte);
  }

  output = std::move(bytes);
  return true;
}

bool decode_nec_packet(const std::vector<uint32_t>& durations, std::vector<uint8_t>& output) {
  for (std::size_t i = 0; i + 2 < durations.size(); ++i) {
    if (decode_nec_packet_from(durations, i, output)) {
      return true;
    }
  }
  return false;
}

std::string summarize_raw(const std::vector<uint32_t>& durations) {
  if (durations.empty()) {
    return {};
  }
  std::ostringstream out;
  const std::size_t count = std::min<std::size_t>(durations.size(), 12);
  for (std::size_t i = 0; i < count; ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << durations[i] << "us";
  }
  if (durations.size() > count) {
    out << " ...";
  }
  return out.str();
}

void finalize_receive(IrReceiveSnapshot& snapshot, const std::vector<uint32_t>& durations) {
  snapshot.signal_seen  = true;
  snapshot.raw_summary  = summarize_raw(durations);
  snapshot.data_changed = false;

  std::vector<uint8_t> decoded;
  if (decode_nec_packet(durations, decoded)) {
    uint16_t address = 0;
    uint16_t command = 0;
    std::string protocol;
    if (!parse_nec_bytes(decoded, address, command, protocol)) {
      snapshot.message = "IR signal received (raw)";
      return;
    }
    const bool matched = !snapshot.address_filter_enabled || address == snapshot.expected_address;

    snapshot.data_changed    = decoded != snapshot.data || address != snapshot.address ||
                             command != snapshot.command || protocol != snapshot.protocol ||
                             matched != snapshot.address_matched;
    snapshot.data            = std::move(decoded);
    snapshot.address         = address;
    snapshot.command         = command;
    snapshot.protocol        = std::move(protocol);
    snapshot.address_matched = matched;
    snapshot.message = matched ? snapshot.protocol + " packet received" : "NEC address mismatch";
    LOG_DEBUG("IR {} received: address={} command={} expected={} matched={}",
              snapshot.protocol,
              format_nec_address(address),
              format_nec_command(command),
              snapshot.address_filter_enabled ? format_nec_address(snapshot.expected_address) : "any",
              matched ? "yes" : "no");
  } else {
    snapshot.message = "IR signal received (raw)";
  }
}

#if APP_USE_LIRC
const char* scancode_protocol_name(uint16_t rc_proto) {
  switch (rc_proto) {
    case RC_PROTO_NEC:
      return K_PROTOCOL_NEC;
    case RC_PROTO_NECX:
      return K_PROTOCOL_NECX;
    case RC_PROTO_NEC32:
      return K_PROTOCOL_NEC32;
    default:
      return "IR";
  }
}

bool decode_scancode(const lirc_scancode& scancode,
                     uint16_t& address,
                     uint16_t& command,
                     std::vector<uint8_t>& data,
                     std::string& protocol) {
  const uint8_t command_byte = static_cast<uint8_t>(scancode.scancode & 0xffU);
  command = make_nec_command(command_byte);

  switch (scancode.rc_proto) {
    case RC_PROTO_NEC: {
      const auto address_byte = static_cast<uint8_t>((scancode.scancode >> 8U) & 0xffU);
      address  = static_cast<uint16_t>(address_byte) |
                 static_cast<uint16_t>(static_cast<uint8_t>(~address_byte) << 8U);
      data     = make_standard_nec_bytes(address_byte, command_byte);
      protocol = K_PROTOCOL_NEC;
      return true;
    }
    case RC_PROTO_NECX: {
      address = static_cast<uint16_t>((scancode.scancode >> 8U) & 0xffffU);
      data    = make_nec_bytes(address, command);
      protocol = K_PROTOCOL_NECX;
      return true;
    }
    case RC_PROTO_NEC32: {
      const uint32_t value = static_cast<uint32_t>(scancode.scancode & 0xffffffffULL);
      address = static_cast<uint16_t>((value >> 16U) & 0xffffU);
      command = static_cast<uint16_t>(value & 0xffffU);
      data    = make_nec_bytes(address, command);
      protocol = K_PROTOCOL_NEC32;
      return true;
    }
    default:
      return false;
  }
}

void finalize_receive_scancode(IrReceiveSnapshot& snapshot, const lirc_scancode& scancode) {
  if (scancode.flags & LIRC_SCANCODE_FLAG_REPEAT) {
    snapshot.signal_seen = true;
    snapshot.message     = "NEC repeat received";
    return;
  }

  uint16_t address = 0;
  uint16_t command = 0;
  std::vector<uint8_t> data;
  std::string protocol;
  if (!decode_scancode(scancode, address, command, data, protocol)) {
    snapshot.signal_seen = true;
    snapshot.message     = "IR scancode received";
    return;
  }

  const bool matched      = !snapshot.address_filter_enabled || address == snapshot.expected_address;
  snapshot.signal_seen    = true;
  snapshot.data_changed   = data != snapshot.data || address != snapshot.address ||
                           command != snapshot.command || protocol != snapshot.protocol ||
                           matched != snapshot.address_matched;
  snapshot.address        = address;
  snapshot.command        = command;
  snapshot.data           = std::move(data);
  snapshot.protocol       = std::move(protocol);
  snapshot.address_matched = matched;
  snapshot.raw_summary    = {};
  snapshot.message        = matched ? snapshot.protocol + " packet received" : "NEC address mismatch";
  LOG_DEBUG("IR {} scancode received: scancode=0x{:x} address={} command={} expected={} matched={}",
            snapshot.protocol,
            static_cast<unsigned long long>(scancode.scancode),
            format_nec_address(address),
            format_nec_command(command),
            snapshot.address_filter_enabled ? format_nec_address(snapshot.expected_address) : "any",
            matched ? "yes" : "no");
}

bool protocols_have_nec_enabled(const std::filesystem::path& protocols_path) {
  std::ifstream stream(protocols_path);
  std::string content;
  std::getline(stream, content);
  return content.find("[nec]") != std::string::npos ||
         content.find("[necx]") != std::string::npos ||
         content.find("[nec32]") != std::string::npos;
}

bool enable_nec_protocols(const char* rc_name) {
  const std::filesystem::path protocols_path =
      std::filesystem::path("/sys/class/rc") / rc_name / "protocols";
  if (protocols_have_nec_enabled(protocols_path)) {
    return true;
  }
  for (const char* protocol : {"+nec", "+necx", "+nec32"}) {
    std::ofstream stream(protocols_path);
    if (!stream) {
      LOG_VERBOSE("IR receiver protocol enable skipped: path={}", protocols_path.string());
      return false;
    }
    stream << protocol;
  }
  return protocols_have_nec_enabled(protocols_path);
}
#endif

}  // namespace

IrDeviceInfo read_receiver_info() { return find_ir_device(false); }

IrDeviceInfo read_sender_info() { return find_ir_device(true); }

IrSendResult send_nec_packet(uint16_t address) {
  IrSendResult result;
  result.address  = address;
  result.command  = random_command();
  result.data     = make_nec_bytes(address, result.command);
  result.protocol = K_PROTOCOL_NEC32;
  const auto encoded = encode_nec_packet(result.data);
  auto info          = read_sender_info();
  result.device_path = info.lirc_path;
  if (!info.available) {
    result.message = info.error_message.empty() ? "IR sender unavailable" : info.error_message;
    LOG_WARN("IR NEC send skipped: {}", result.message);
    return result;
  }

#if APP_USE_LIRC
  LOG_DEBUG("IR NEC32 send requested: path={} address={} command={} data={}",
            info.lirc_path,
            format_nec_address(result.address),
            format_nec_command(result.command),
            format_hex_bytes(result.data));
  int fd = open(info.lirc_path.c_str(), O_WRONLY);
  if (fd < 0) {
    result.message = errno_message("Open sender failed");
    LOG_WARN("IR sender open failed: {}", result.message);
    return result;
  }

  if (info.features & LIRC_CAN_SET_SEND_CARRIER) {
    uint32_t carrier = K_CARRIER_HZ;
    ioctl(fd, LIRC_SET_SEND_CARRIER, &carrier);
  }
  if (info.features & LIRC_CAN_SET_SEND_DUTY_CYCLE) {
    uint32_t duty_cycle = K_DUTY_CYCLE;
    ioctl(fd, LIRC_SET_SEND_DUTY_CYCLE, &duty_cycle);
  }

  uint32_t send_mode = (info.features & LIRC_CAN_SEND_PULSE) ? LIRC_MODE_PULSE : LIRC_MODE_RAW;
  ioctl(fd, LIRC_SET_SEND_MODE, &send_mode);

  const ssize_t bytes_written = write(fd, encoded.data(), encoded.size() * sizeof(encoded[0]));
  const int write_errno       = errno;
  close(fd);

  if (bytes_written < 0) {
    errno          = write_errno;
    result.message = errno_message("Send failed");
    LOG_WARN("IR NEC send failed: {}", result.message);
    return result;
  }
  if (static_cast<std::size_t>(bytes_written) != encoded.size() * sizeof(encoded[0])) {
    result.message = "Short write while sending NEC packet";
    LOG_WARN("IR NEC send failed: {}", result.message);
    return result;
  }

  result.success = true;
  result.message = "NEC32 packet sent";
  LOG_INFO("IR NEC32 packet sent: path={} address={} command={} data={}",
           info.lirc_path,
           format_nec_address(result.address),
           format_nec_command(result.command),
           format_hex_bytes(result.data));
#else
  result.message = "liblirc-dev support was not found at build time";
#endif
  return result;
}

std::string format_hex_bytes(const std::vector<uint8_t>& data) {
  if (data.empty()) {
    return "--";
  }

  std::ostringstream out;
  out << std::uppercase << std::hex;
  for (std::size_t i = 0; i < data.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "0x%02X", data[i]);
    out << buffer;
  }
  return out.str();
}

std::string format_nec_address(uint16_t address) {
  char buffer[12];
  std::snprintf(buffer, sizeof(buffer), "0x%04X", address);
  return buffer;
}

std::string format_nec_command(uint16_t command) {
  char buffer[12];
  std::snprintf(buffer, sizeof(buffer), "0x%04X", command);
  return buffer;
}

IrReceiverSession::IrReceiverSession() = default;

IrReceiverSession::~IrReceiverSession() { stop(); }

bool IrReceiverSession::start(uint16_t expected_address, bool filter_address) {
  if (fd_ >= 0) {
    snapshot_.expected_address        = expected_address;
    snapshot_.address_filter_enabled  = filter_address;
    return true;
  }

  snapshot_                         = {};
  snapshot_.expected_address        = expected_address;
  snapshot_.address_filter_enabled  = filter_address;
  auto info                         = read_receiver_info();
  snapshot_.device_path             = info.lirc_path;
  if (!info.available) {
    snapshot_.message = info.error_message.empty() ? "IR receiver unavailable" : info.error_message;
    LOG_WARN("IR receiver unavailable: {}", snapshot_.message);
    return false;
  }

#if APP_USE_LIRC
  const bool nec_scancode_ready = enable_nec_protocols(info.rc_name.c_str());
  LOG_DEBUG("opening IR receiver: path={} expected_address={}",
            info.lirc_path,
            format_nec_address(expected_address));
  fd_ = open(info.lirc_path.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd_ < 0) {
    snapshot_.message = errno_message("Open receiver failed");
    LOG_WARN("IR receiver open failed: {}", snapshot_.message);
    return false;
  }

  scancode_mode_ = false;
  mode2_         = (info.features & LIRC_CAN_REC_MODE2) != 0;
  uint32_t rec_mode = LIRC_MODE_RAW;
  const bool has_raw_receive = (info.features & (LIRC_CAN_REC_MODE2 | LIRC_CAN_REC_RAW)) != 0;
  if ((info.features & LIRC_CAN_REC_SCANCODE) && (nec_scancode_ready || !has_raw_receive)) {
    rec_mode = LIRC_MODE_SCANCODE;
    if (ioctl(fd_, LIRC_SET_REC_MODE, &rec_mode) == 0) {
      scancode_mode_ = true;
    } else {
      LOG_DEBUG("IR receiver scancode mode unavailable, falling back to raw mode: {}",
                std::strerror(errno));
    }
  }
  if (!scancode_mode_) {
    rec_mode = mode2_ ? LIRC_MODE_MODE2 : LIRC_MODE_RAW;
    ioctl(fd_, LIRC_SET_REC_MODE, &rec_mode);
  }

  if (!scancode_mode_ && (info.features & LIRC_CAN_SET_REC_TIMEOUT)) {
    uint32_t timeout = K_REC_TIMEOUT_US;
    ioctl(fd_, LIRC_SET_REC_TIMEOUT, &timeout);
    uint32_t enable_reports = 1;
    ioctl(fd_, LIRC_SET_REC_TIMEOUT_REPORTS, &enable_reports);
  }

  snapshot_.opened  = true;
  snapshot_.message = "Waiting for NEC data";
  pulse_space_buffer_.clear();
  LOG_INFO("IR receiver opened: path={} mode={} expected_address={}",
           info.lirc_path,
           scancode_mode_ ? "scancode" : (mode2_ ? "mode2" : "raw"),
           format_nec_address(expected_address));
  return true;
#else
  snapshot_.message = "liblirc-dev support was not found at build time";
  return false;
#endif
}

void IrReceiverSession::stop() {
  if (fd_ >= 0) {
    LOG_DEBUG("closing IR receiver: path={}", snapshot_.device_path);
  }
  close_fd(fd_);
  scancode_mode_ = false;
  pulse_space_buffer_.clear();
  if (snapshot_.opened) {
    snapshot_.opened = false;
  }
}

IrReceiveSnapshot IrReceiverSession::poll() {
  snapshot_.data_changed = false;
  if (fd_ < 0) {
    return snapshot_;
  }

#if APP_USE_LIRC
  if (scancode_mode_) {
    std::array<lirc_scancode, 8> scancodes{};
    while (true) {
      const ssize_t bytes_read = read(fd_, scancodes.data(), scancodes.size() * sizeof(scancodes[0]));
      if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
          break;
        }
        snapshot_.message = errno_message("Receive failed");
        stop();
        return snapshot_;
      }
      if (bytes_read == 0) {
        break;
      }

      const std::size_t count = static_cast<std::size_t>(bytes_read) / sizeof(scancodes[0]);
      for (std::size_t i = 0; i < count; ++i) {
        finalize_receive_scancode(snapshot_, scancodes[i]);
      }
    }
    return snapshot_;
  }

  std::array<uint32_t, 96> words{};
  while (true) {
    const ssize_t bytes_read = read(fd_, words.data(), words.size() * sizeof(words[0]));
    if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        break;
      }
      snapshot_.message = errno_message("Receive failed");
      stop();
      return snapshot_;
    }
    if (bytes_read == 0) {
      break;
    }

    const std::size_t count = static_cast<std::size_t>(bytes_read) / sizeof(words[0]);
    for (std::size_t i = 0; i < count; ++i) {
      const uint32_t word = words[i];
      if (mode2_) {
        if (LIRC_IS_TIMEOUT(word) || LIRC_IS_OVERFLOW(word)) {
          if (!pulse_space_buffer_.empty()) {
            finalize_receive(snapshot_, pulse_space_buffer_);
            pulse_space_buffer_.clear();
          }
          continue;
        }
        if (LIRC_IS_SPACE(word) || LIRC_IS_PULSE(word)) {
          const uint32_t value = LIRC_VALUE(word);
          if (LIRC_IS_SPACE(word) && value > 12000 && !pulse_space_buffer_.empty()) {
            finalize_receive(snapshot_, pulse_space_buffer_);
            pulse_space_buffer_.clear();
          }
          pulse_space_buffer_.push_back(value);
        }
      } else {
        pulse_space_buffer_.push_back(word);
      }

      if (pulse_space_buffer_.size() >= 2 + K_PACKET_BYTES * 16 + 1) {
        std::vector<uint8_t> decoded;
        if (decode_nec_packet(pulse_space_buffer_, decoded)) {
          finalize_receive(snapshot_, pulse_space_buffer_);
          pulse_space_buffer_.clear();
        }
      }
      if (pulse_space_buffer_.size() > 160) {
        finalize_receive(snapshot_, pulse_space_buffer_);
        pulse_space_buffer_.clear();
      }
    }
  }
#endif
  return snapshot_;
}

const IrReceiveSnapshot& IrReceiverSession::snapshot() const { return snapshot_; }

}  // namespace platform::ir
