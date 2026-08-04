/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "factory_upload_service.h"

#if defined(FACTORY_TEST_SCONS_BUILD)
#include "factory_test_config.h"
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

#include "logger.h"
#include "serialization.h"
#include "uart_service.h"

namespace platform::factory_upload {
namespace {

using Clock = std::chrono::steady_clock;

constexpr const char* K_FACTORY_PREFIX     = "M5FACTORY:";
constexpr const char* K_HANDSHAKE_COMMAND  = "M5FACTORY:HANDSHAKE";
constexpr const char* K_ACK_COMMAND        = "M5FACTORY:ACK";
constexpr const char* K_RESULT_COMMAND     = "M5FACTORY:RESULT";
constexpr const char* K_RESULT_ACK_COMMAND = "M5FACTORY:ACK_RESULT";
constexpr std::size_t K_MAX_PROTOCOL_LINE  = 511;
constexpr std::size_t K_MAX_RX_LINE        = 1024;
constexpr auto K_DISCOVERY_RETRY           = std::chrono::milliseconds(1000);
constexpr auto K_IO_POLL                   = std::chrono::milliseconds(20);
constexpr auto K_LINK_HEARTBEAT_INTERVAL   = std::chrono::seconds(1);
constexpr auto K_LISTENER_HEARTBEAT_STALE  = std::chrono::seconds(3);
constexpr auto K_INITIAL_HANDSHAKE_WARNING = std::chrono::seconds(10);
constexpr auto K_QUEUED_HANDSHAKE_TIMEOUT  = std::chrono::seconds(10);
constexpr auto K_RESULT_ACK_TIMEOUT        = std::chrono::seconds(60);

std::int64_t heartbeat_milliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch())
      .count();
}

bool command_matches(const std::string& line, const char* command) {
  const std::string command_text = command ? command : "";
  return line.compare(0, command_text.size(), command_text) == 0 &&
         (line.size() == command_text.size() || line[command_text.size()] == ' ');
}

std::string payload_after_command(const std::string& line, const char* command) {
  std::size_t offset = command ? std::char_traits<char>::length(command) : 0;
  while (offset < line.size() && line[offset] == ' ') {
    ++offset;
  }
  return line.substr(offset);
}

const serialization::OutputValue* object_field(const serialization::OutputValue& object,
                                               const char* key) {
  if (object.type != serialization::OutputValue::Type::Object || !key) {
    return nullptr;
  }
  const auto it = object.object_values.find(key);
  return it == object.object_values.end() ? nullptr : &it->second;
}

bool parse_handshake(const std::string& payload,
                     std::optional<long long>& sequence,
                     std::string& error_message) {
  sequence.reset();
  const auto parsed = serialization::parse_json_output(payload);
  if (!parsed.success() || parsed.value.type != serialization::OutputValue::Type::Object) {
    error_message = parsed.success() ? "handshake payload is not a JSON object"
                                     : "invalid handshake JSON: " + parsed.error_message;
    return false;
  }

  const auto* protocol = object_field(parsed.value, "proto");
  if (!protocol || protocol->type != serialization::OutputValue::Type::Number ||
      std::fabs(protocol->number_value - 1.0) > 0.0001) {
    error_message = "handshake proto must be 1";
    return false;
  }

  const auto* role = object_field(parsed.value, "role");
  if (!role || role->type != serialization::OutputValue::Type::String ||
      role->string_value != "tab5") {
    error_message = "handshake role must be tab5";
    return false;
  }

  if (const auto* seq = object_field(parsed.value, "seq")) {
    if (seq->type != serialization::OutputValue::Type::Number ||
        !std::isfinite(seq->number_value)) {
      error_message = "handshake seq must be a number";
      return false;
    }
    sequence = static_cast<long long>(seq->number_value);
  }
  error_message.clear();
  return true;
}

bool parse_result_ack(const std::string& payload,
                      bool& saved,
                      std::string& status,
                      std::string& message,
                      std::string& error_message) {
  saved = false;
  status.clear();
  message.clear();
  const auto parsed = serialization::parse_json_output(payload);
  if (!parsed.success() || parsed.value.type != serialization::OutputValue::Type::Object) {
    error_message = parsed.success() ? "ACK_RESULT payload is not a JSON object"
                                     : "invalid ACK_RESULT JSON: " + parsed.error_message;
    return false;
  }

  const auto* saved_field = object_field(parsed.value, "saved");
  if (!saved_field || saved_field->type != serialization::OutputValue::Type::Boolean) {
    error_message = "ACK_RESULT saved must be a boolean";
    return false;
  }
  saved = saved_field->bool_value;

  if (const auto* status_field = object_field(parsed.value, "status");
      status_field && status_field->type == serialization::OutputValue::Type::String) {
    status = status_field->string_value;
  }
  if (const auto* message_field = object_field(parsed.value, "message");
      message_field && message_field->type == serialization::OutputValue::Type::String) {
    message = message_field->string_value;
  }
  error_message.clear();
  return true;
}

}  // namespace

struct FactoryUploadService::Impl {
  explicit Impl(FactoryUploadConfig upload_config)
      : config(std::move(upload_config)) {}

  ~Impl() { stop_worker(); }

  void start_worker() {
    std::lock_guard<std::mutex> lock(mutex);
    if (worker.joinable()) {
      return;
    }
    stop_requested = false;
    LOG_INFO("factory upload initializing vid={:04x} pid={:04x} baud={}",
             config.vendor_id,
             config.product_id,
             config.baud_rate);
#if !USE_DESKTOP
    worker = std::thread([this]() { run(); });
#else
    set_snapshot_locked(UploadState::UNAVAILABLE,
                        "Factory upload is unavailable in desktop builds",
                        "");
    LOG_WARN("factory upload disabled: libserialport backend is not enabled");
#endif
  }

  void stop_worker() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      stop_requested = true;
    }
    wake.notify_all();
    if (worker.joinable()) {
      worker.join();
    }
  }

  bool queue(std::string payload, std::string& error_message) {
    error_message.clear();
    payload.erase(std::remove(payload.begin(), payload.end(), '\r'), payload.end());
    payload.erase(std::remove(payload.begin(), payload.end(), '\n'), payload.end());
    if (payload.empty()) {
      error_message = "Test result payload is empty";
      return false;
    }
    const std::size_t line_size =
        std::char_traits<char>::length(K_RESULT_COMMAND) + 1 + payload.size();
    if (line_size > K_MAX_PROTOCOL_LINE) {
      error_message = "Test result exceeds the 511-byte protocol line limit";
      LOG_ERROR("factory upload rejected payload: line_bytes={} limit={}",
                line_size,
                K_MAX_PROTOCOL_LINE);
      return false;
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      if (!worker.joinable()) {
        error_message = "Factory upload service is unavailable";
        return false;
      }
      const auto now            = heartbeat_milliseconds();
      const auto last_heartbeat = listener_heartbeat_ms.load(std::memory_order_acquire);
      const bool heartbeat_fresh =
          last_heartbeat > 0 && now >= last_heartbeat &&
          now - last_heartbeat <=
              std::chrono::duration_cast<std::chrono::milliseconds>(K_LISTENER_HEARTBEAT_STALE)
                  .count();
      if (!listener_running.load(std::memory_order_acquire) || !heartbeat_fresh) {
        error_message = "Factory upload listener is offline";
        LOG_ERROR("factory upload request rejected: listener heartbeat age={}ms",
                  last_heartbeat > 0 && now >= last_heartbeat ? now - last_heartbeat : -1);
        return false;
      }
      if (pending_payload || request_active) {
        error_message = "A test result upload is already in progress";
        return false;
      }
      pending_payload  = std::move(payload);
      pending_deadline = Clock::now() + K_QUEUED_HANDSHAKE_TIMEOUT;
      request_active   = true;
      set_snapshot_locked(UploadState::QUEUED, "Test result queued for upload", snapshot.port);
    }
    LOG_INFO("factory upload queued result payload_bytes={}", line_size);
    wake.notify_all();
    return true;
  }

  UploadSnapshot get_snapshot() const {
    std::lock_guard<std::mutex> lock(mutex);
    auto result                      = snapshot;
    const auto now                   = heartbeat_milliseconds();
    const auto last_heartbeat        = listener_heartbeat_ms.load(std::memory_order_acquire);
    result.listener_heartbeat_age_ms = last_heartbeat <= 0 || now <= last_heartbeat
                                           ? 0
                                           : static_cast<std::uint64_t>(now - last_heartbeat);
    result.listener_online =
        listener_running.load(std::memory_order_acquire) &&
        result.listener_heartbeat_age_ms <=
            static_cast<std::uint64_t>(K_LISTENER_HEARTBEAT_STALE.count() * 1000);
    return result;
  }

  bool should_stop() const {
    std::lock_guard<std::mutex> lock(mutex);
    return stop_requested;
  }

  void wait_for(std::chrono::milliseconds duration, bool wake_on_pending = true) {
    std::unique_lock<std::mutex> lock(mutex);
    wake.wait_for(lock, duration, [this, wake_on_pending]() {
      return stop_requested || (wake_on_pending && pending_payload.has_value());
    });
  }

  void publish(UploadState state, std::string message, std::string port = {}) {
    std::lock_guard<std::mutex> lock(mutex);
    set_snapshot_locked(state, std::move(message), std::move(port));
  }

  void set_snapshot_locked(UploadState state, std::string message, std::string port) {
    if (snapshot.state == state && snapshot.message == message && snapshot.port == port) {
      return;
    }
    snapshot.state   = state;
    snapshot.message = std::move(message);
    snapshot.port    = std::move(port);
    ++snapshot.revision;
  }

  bool has_terminal_upload_snapshot() const {
    std::lock_guard<std::mutex> lock(mutex);
    return snapshot.state == UploadState::SUCCEEDED || snapshot.state == UploadState::FAILED;
  }

  std::optional<std::string> take_pending_payload() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!pending_payload) {
      return std::nullopt;
    }
    auto result = std::move(pending_payload);
    pending_payload.reset();
    return result;
  }

  bool expire_pending_request(const std::string& message, const std::string& port) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (!pending_payload || Clock::now() <= pending_deadline) {
        return false;
      }
      pending_payload.reset();
      request_active = false;
      set_snapshot_locked(UploadState::FAILED, message, port);
    }
    LOG_ERROR("factory upload failed: {}", message);
    return true;
  }

  bool fail_pending_request(const std::string& message, const std::string& port) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (!pending_payload) {
        return false;
      }
      pending_payload.reset();
      request_active = false;
      set_snapshot_locked(UploadState::FAILED, message, port);
    }
    LOG_ERROR("factory upload failed: {}", message);
    return true;
  }

  void finish_request(UploadState state, std::string message, const std::string& port) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      request_active = false;
      set_snapshot_locked(state, message, port);
    }
    if (state == UploadState::SUCCEEDED) {
      LOG_INFO("factory upload completed: {}", message);
    } else {
      LOG_ERROR("factory upload failed: {}", message);
    }
  }

  bool send_handshake_ack(connectivity::UartDebugSession& session,
                          const std::string& line,
                          const std::string& port) {
    std::optional<long long> sequence;
    std::string error;
    if (!parse_handshake(payload_after_command(line, K_HANDSHAKE_COMMAND), sequence, error)) {
      LOG_ERROR("factory upload handshake rejected port={}: {}", port, error);
      return false;
    }

    std::string ack = std::string(K_ACK_COMMAND) +
                      " {\"proto\":1,\"role\":\"tester\",\"model\":\"" + config.tester_model +
                      "\",\"fw\":\"" + config.tester_firmware + "\"";
    if (sequence) {
      ack += ",\"seq\":" + std::to_string(*sequence);
    }
    ack += "}\n";
    if (!session.write_text(ack, error)) {
      LOG_ERROR("factory upload handshake ACK write failed port={}: {}", port, error);
      return false;
    }

    LOG_INFO("factory upload handshake complete port={} seq={}",
             port,
             sequence ? std::to_string(*sequence) : "none");
    return true;
  }

  void run() {
    listener_running.store(true, std::memory_order_release);
    listener_heartbeat_ms.store(heartbeat_milliseconds(), std::memory_order_release);
    LOG_INFO("factory upload listener online");

    std::unique_ptr<connectivity::UartDebugSession> session;
    connectivity::UsbSerialPortInfo device;
    std::string port;
    std::string rx_buffer;
    std::string last_discovery_error;
    std::string last_open_error;
    std::optional<std::string> active_payload;
    bool handshaked                = false;
    bool waiting_result_ack        = false;
    bool handshake_warning_emitted = false;
    Clock::time_point connected_at{};
    Clock::time_point next_link_heartbeat{};
    Clock::time_point request_deadline{};
    Clock::time_point result_deadline{};

    while (!should_stop()) {
      listener_heartbeat_ms.store(heartbeat_milliseconds(), std::memory_order_release);
      if (!session) {
        std::string error;
        device = connectivity::find_usb_serial_port(config.vendor_id, config.product_id, error);
        port   = device.path;
        if (port.empty()) {
          if (error != last_discovery_error) {
            if (error == "matching USB serial port not found") {
              LOG_INFO("factory upload waiting for USB serial vid={:04x} pid={:04x}",
                       config.vendor_id,
                       config.product_id);
            } else {
              LOG_ERROR("factory upload USB serial enumeration failed: {}", error);
            }
            last_discovery_error = error;
          }
          if (expire_pending_request("USB serial device 303a:1001 was not found", "")) {
            wait_for(std::chrono::duration_cast<std::chrono::milliseconds>(K_DISCOVERY_RETRY),
                     false);
            continue;
          }
          if (!has_terminal_upload_snapshot()) {
            publish(UploadState::SEARCHING, "Waiting for USB serial device 303a:1001", "");
          }
          wait_for(std::chrono::duration_cast<std::chrono::milliseconds>(K_DISCOVERY_RETRY), false);
          continue;
        }

        connectivity::UartOpenResult open_result;
        session = connectivity::UartDebugSession::open(port, config.baud_rate, open_result, false);
        if (!session) {
          if (open_result.message != last_open_error) {
            LOG_ERROR("factory upload serial open failed port={} baud={}: {}",
                      port,
                      config.baud_rate,
                      open_result.message);
            last_open_error = open_result.message;
          }
          if (!fail_pending_request("Failed to open factory upload serial port", port) &&
              !has_terminal_upload_snapshot()) {
            publish(UploadState::SEARCHING, "Failed to open factory upload serial port", port);
          }
          wait_for(std::chrono::duration_cast<std::chrono::milliseconds>(K_DISCOVERY_RETRY), false);
          continue;
        }

        last_discovery_error.clear();
        last_open_error.clear();
        LOG_INFO("factory upload serial ready port={} usb={}:{} vid={:04x} pid={:04x} baud={}",
                 port,
                 device.bus,
                 device.address,
                 config.vendor_id,
                 config.product_id,
                 config.baud_rate);
        handshaked                = false;
        waiting_result_ack        = false;
        handshake_warning_emitted = false;
        connected_at              = Clock::now();
        next_link_heartbeat       = connected_at + K_LINK_HEARTBEAT_INTERVAL;
        rx_buffer.clear();
        if (!has_terminal_upload_snapshot()) {
          publish(UploadState::WAITING_HANDSHAKE, "Waiting for M5FACTORY handshake", port);
        }
      }

      const auto before_io = Clock::now();
      if (before_io >= next_link_heartbeat) {
        std::string heartbeat_error;
        const auto heartbeat_device = connectivity::find_usb_serial_port(config.vendor_id,
                                                                         config.product_id,
                                                                         heartbeat_error);
        next_link_heartbeat         = before_io + K_LINK_HEARTBEAT_INTERVAL;
        if (!connectivity::same_usb_serial_instance(device, heartbeat_device)) {
          LOG_WARN(
              "factory upload link heartbeat lost port={} usb={}:{} detected_port={} "
              "detected_usb={}:{} error={}",
              port,
              device.bus,
              device.address,
              heartbeat_device.path.empty() ? "none" : heartbeat_device.path,
              heartbeat_device.bus,
              heartbeat_device.address,
              heartbeat_error.empty() ? "none" : heartbeat_error);
          const bool upload_was_active = active_payload.has_value() || waiting_result_ack;
          if (upload_was_active) {
            finish_request(UploadState::FAILED, "USB serial device disconnected", port);
          }
          session->close(false);
          session.reset();
          device = {};
          active_payload.reset();
          handshaked         = false;
          waiting_result_ack = false;
          rx_buffer.clear();
          if (!upload_was_active && !has_terminal_upload_snapshot()) {
            publish(UploadState::SEARCHING, "Factory upload device disconnected", "");
          }
          continue;
        }
      }

      if (!active_payload) {
        active_payload = take_pending_payload();
        if (active_payload) {
          request_deadline = Clock::now() + K_QUEUED_HANDSHAKE_TIMEOUT;
          LOG_INFO("factory upload request active port={} handshaked={}", port, handshaked);
        }
      }

      std::string io_error;
      const auto received = session->read_available(io_error);
      if (!io_error.empty()) {
        LOG_ERROR("factory upload serial read failed port={}: {}", port, io_error);
        if (active_payload || waiting_result_ack) {
          finish_request(UploadState::FAILED, "USB serial read failed", port);
        }
        session->close(false);
        session.reset();
        device = {};
        active_payload.reset();
        waiting_result_ack = false;
        continue;
      }

      for (const char character : received) {
        if (character == '\r') {
          continue;
        }
        if (character != '\n') {
          rx_buffer.push_back(character);
          if (rx_buffer.size() > K_MAX_RX_LINE) {
            LOG_ERROR("factory upload RX line exceeded {} bytes; discarding", K_MAX_RX_LINE);
            rx_buffer.clear();
          }
          continue;
        }

        const auto line = std::move(rx_buffer);
        rx_buffer.clear();
        if (line.empty()) {
          continue;
        }
        if (command_matches(line, K_HANDSHAKE_COMMAND)) {
          if (send_handshake_ack(*session, line, port)) {
            handshaked = true;
            if (!active_payload && !waiting_result_ack && !has_terminal_upload_snapshot()) {
              publish(UploadState::READY, "Factory upload handshake ready", port);
            }
          } else if (active_payload) {
            finish_request(UploadState::FAILED, "Factory upload handshake failed", port);
            active_payload.reset();
          }
          continue;
        }
        if (command_matches(line, K_RESULT_ACK_COMMAND)) {
          if (!waiting_result_ack) {
            LOG_WARN("factory upload received unexpected ACK_RESULT port={}", port);
            continue;
          }
          bool saved = false;
          std::string status;
          std::string message;
          std::string error;
          if (!parse_result_ack(payload_after_command(line, K_RESULT_ACK_COMMAND),
                                saved,
                                status,
                                message,
                                error)) {
            finish_request(UploadState::FAILED, error, port);
          } else if (!saved) {
            finish_request(UploadState::FAILED,
                           "Lower device rejected test result" +
                               (message.empty() ? std::string{} : ": " + message),
                           port);
          } else {
            finish_request(UploadState::SUCCEEDED, "Test result uploaded", port);
          }
          LOG_INFO("factory upload ACK_RESULT port={} saved={} status={} message={}",
                   port,
                   saved,
                   status.empty() ? "--" : status,
                   message.empty() ? "--" : message);
          waiting_result_ack = false;
          active_payload.reset();
          continue;
        }
        if (line.compare(0, std::char_traits<char>::length(K_FACTORY_PREFIX), K_FACTORY_PREFIX) ==
            0) {
          LOG_WARN("factory upload received unexpected protocol line port={} command={}",
                   port,
                   line.substr(0, line.find(' ')));
        }
      }

      const auto now = Clock::now();
      if (!handshaked && !handshake_warning_emitted &&
          now - connected_at >= K_INITIAL_HANDSHAKE_WARNING) {
        handshake_warning_emitted = true;
        LOG_WARN("factory upload handshake not received port={} after {}s",
                 port,
                 K_INITIAL_HANDSHAKE_WARNING.count());
      }

      if (active_payload && !waiting_result_ack && handshaked) {
        publish(UploadState::SENDING, "Sending test result", port);
        const std::string result_line =
            std::string(K_RESULT_COMMAND) + " " + *active_payload + "\n";
        if (!session->write_text(result_line, io_error)) {
          finish_request(UploadState::FAILED, "Failed to send test result: " + io_error, port);
          active_payload.reset();
        } else {
          waiting_result_ack = true;
          result_deadline    = Clock::now() + K_RESULT_ACK_TIMEOUT;
          publish(UploadState::WAITING_RESULT_ACK, "Waiting for upload acknowledgement", port);
          LOG_INFO("factory upload RESULT sent port={} bytes={}", port, result_line.size());
        }
      } else if (active_payload && !waiting_result_ack && now > request_deadline) {
        LOG_ERROR("factory upload handshake timeout port={}", port);
        finish_request(UploadState::FAILED,
                       "Handshake timeout; retry handshake on the lower device",
                       port);
        active_payload.reset();
      } else if (waiting_result_ack && now > result_deadline) {
        LOG_ERROR("factory upload ACK_RESULT timeout port={}", port);
        finish_request(UploadState::FAILED, "Timed out waiting for ACK_RESULT", port);
        waiting_result_ack = false;
        active_payload.reset();
      }

      wait_for(std::chrono::duration_cast<std::chrono::milliseconds>(K_IO_POLL));
    }

    listener_running.store(false, std::memory_order_release);
    listener_heartbeat_ms.store(heartbeat_milliseconds(), std::memory_order_release);
    LOG_INFO("factory upload listener offline");
  }

  FactoryUploadConfig config;
  mutable std::mutex mutex;
  std::condition_variable wake;
  std::thread worker;
  std::atomic<bool> listener_running{false};
  std::atomic<std::int64_t> listener_heartbeat_ms{0};
  bool stop_requested{false};
  bool request_active{false};
  std::optional<std::string> pending_payload{};
  Clock::time_point pending_deadline{};
  UploadSnapshot snapshot{};
};

FactoryUploadService::FactoryUploadService(FactoryUploadConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

FactoryUploadService::~FactoryUploadService() = default;

void FactoryUploadService::start() { impl_->start_worker(); }

void FactoryUploadService::stop() { impl_->stop_worker(); }

bool FactoryUploadService::queue_result(std::string payload, std::string& error_message) {
  return impl_->queue(std::move(payload), error_message);
}

UploadSnapshot FactoryUploadService::snapshot() const { return impl_->get_snapshot(); }

}  // namespace platform::factory_upload
