/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "py32_upgrade_service.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "device_info_service.h"
#include "logger.h"
#include "process_service.h"

namespace platform::py32_upgrade {
namespace {

constexpr const char* K_ARCHIVE_NAME   = "cardputerzero_ioe1_upgrade-main.tar.gz";
constexpr const char* K_PACKAGE_DIR    = "cardputerzero_ioe1_upgrade-main";
constexpr const char* K_SCRIPT_NAME    = "upgrade_io1.py";
constexpr const char* K_FIRMWARE_NAME  = "app_without_flash_Z_48MHZ(0x4F~0x50)_0xF8.h";
constexpr const char* K_TARGET_VERSION = "0xF8";

class TemporaryDirectory {
 public:
  explicit TemporaryDirectory(std::filesystem::path path)
      : path_(std::move(path)) {}
  ~TemporaryDirectory() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

int process_id() {
#if defined(_WIN32)
  return _getpid();
#else
  return static_cast<int>(getpid());
#endif
}

std::string shell_quote(const std::string& value) {
  std::string quoted{"'"};
  for (const char ch : value) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted += ch;
    }
  }
  quoted += '\'';
  return quoted;
}

void publish(const ProgressCallback& callback, int percent, const char* status) {
  if (callback) {
    callback({std::clamp(percent, 0, 100), status ? status : ""});
  }
}

std::string process_error(const process::ProcessResult& result, const char* fallback) {
  if (!result.error_message.empty()) {
    return result.error_message;
  }
  if (result.timed_out) {
    return "Command timed out";
  }
  if (!result.stderr_text.empty()) {
    auto text = result.stderr_text;
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
      text.pop_back();
    }
    const auto failed = text.rfind("RESULT: FAILED");
    if (failed != std::string::npos) {
      const auto line_end = text.find_first_of("\r\n", failed);
      return text.substr(failed,
                         line_end == std::string::npos ? std::string::npos : line_end - failed);
    }
    const auto line_start = text.find_last_of("\r\n");
    return text.substr(line_start == std::string::npos ? 0 : line_start + 1);
  }
  return fallback ? fallback : "Command failed";
}

bool verify_checksums(const std::filesystem::path& directory,
                      const std::filesystem::path& checksum_file,
                      std::string& error_message) {
  process::ProcessOptions options;
  options.timeout_ms = 30000;
  const auto command = "cd " + shell_quote(directory.string()) + " && sha256sum -c " +
                       shell_quote(checksum_file.filename().string());
  const auto result  = process::run_shell(command, options);
  if (result.success()) {
    return true;
  }
  error_message = process_error(result, "SHA256 verification failed");
  return false;
}

bool run_command(const std::string& executable,
                 const std::vector<std::string>& args,
                 int timeout_ms,
                 const std::string& stdin_text,
                 const process::OutputLineHandler& line_handler,
                 std::string& error_message) {
  process::ProcessOptions options;
  options.timeout_ms          = timeout_ms;
  options.stdin_text          = stdin_text;
  options.stdout_line_handler = line_handler;
  options.stderr_line_handler = line_handler;
  const auto result           = process::run_command(executable, args, options);
  if (result.success()) {
    return true;
  }
  error_message = process_error(result, "Command failed");
  return false;
}

std::filesystem::path make_temporary_directory(std::string& error_message) {
  std::error_code ec;
  const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto path      = std::filesystem::temp_directory_path(ec) /
                         ("factory_test_py32_upgrade_" + std::to_string(process_id()) + "_" +
                          std::to_string(timestamp));
  if (ec || !std::filesystem::create_directories(path, ec)) {
    error_message = ec ? ec.message() : "Cannot create temporary directory";
    return {};
  }
  return path;
}

}  // namespace

Result run(const std::filesystem::path& archive_path, const ProgressCallback& progress_callback) {
  Result result;
  publish(progress_callback, 2, "Checking current version...");
  result.previous_version = device_info::read_py32_firmware_version(true);

  if (archive_path.empty() || archive_path.filename() != K_ARCHIVE_NAME ||
      !std::filesystem::is_regular_file(archive_path)) {
    result.error_status  = "Firmware package not found";
    result.error_message = "Firmware package not found";
    return result;
  }

  auto checksum_path = archive_path;
  checksum_path += ".txt";
  if (!std::filesystem::is_regular_file(checksum_path)) {
    result.error_status  = "Firmware checksum file not found";
    result.error_message = "Firmware checksum file not found";
    return result;
  }

  publish(progress_callback, 10, "Verifying firmware package...");
  if (!verify_checksums(archive_path.parent_path(), checksum_path, result.error_message)) {
    result.error_status = "Firmware package verification failed";
    LOG_ERROR("PY32 archive verification failed: {}", result.error_message);
    return result;
  }

  auto temp_path = make_temporary_directory(result.error_message);
  if (temp_path.empty()) {
    result.error_status = "Temporary directory creation failed";
    return result;
  }
  TemporaryDirectory temporary_directory(temp_path);

  publish(progress_callback, 20, "Extracting firmware package...");
  if (!run_command("tar",
                   {"-xzf", archive_path.string(), "-C", temporary_directory.path().string()},
                   30000,
                   {},
                   {},
                   result.error_message)) {
    result.error_status = "Firmware package extraction failed";
    LOG_ERROR("PY32 archive extraction failed: {}", result.error_message);
    return result;
  }

  const auto package_path       = temporary_directory.path() / K_PACKAGE_DIR;
  const auto internal_checksums = package_path / "SHA256SUMS.txt";
  if (!std::filesystem::is_regular_file(internal_checksums)) {
    result.error_status  = "Extracted firmware checksums not found";
    result.error_message = "Extracted firmware checksums not found";
    return result;
  }

  publish(progress_callback, 30, "Verifying extracted firmware...");
  if (!verify_checksums(package_path, internal_checksums, result.error_message)) {
    result.error_status = "Extracted firmware verification failed";
    LOG_ERROR("PY32 extracted firmware verification failed: {}", result.error_message);
    return result;
  }

  const auto script_path   = package_path / K_SCRIPT_NAME;
  const auto firmware_path = package_path / "firmwares" / K_FIRMWARE_NAME;
  if (!std::filesystem::is_regular_file(script_path) ||
      !std::filesystem::is_regular_file(firmware_path)) {
    result.error_status  = "Upgrade files not found";
    result.error_message = "PY32 upgrade script or 0xF8 firmware not found";
    return result;
  }

  publish(progress_callback, 38, "Checking 0xF8 firmware...");
  if (!run_command("python3",
                   {"-u", script_path.string(), "-f", firmware_path.string(), "--firmware-only"},
                   30000,
                   {},
                   {},
                   result.error_message)) {
    result.error_status = "Firmware validation failed";
    LOG_ERROR("PY32 firmware-only check failed: {}", result.error_message);
    return result;
  }

  publish(progress_callback, 45, "Running upgrade dry run...");
  if (!run_command("python3",
                   {"-u",
                    script_path.string(),
                    "-b",
                    "/dev/i2c-1",
                    "-f",
                    firmware_path.string(),
                    "--dry-run",
                    "-v"},
                   30000,
                   {},
                   {},
                   result.error_message)) {
    result.error_status = "Upgrade dry run failed";
    LOG_ERROR("PY32 dry run failed: {}", result.error_message);
    return result;
  }

  publish(progress_callback, 55, "Upgrading firmware. The screen may go black...");
  const auto upgrade_line_handler = [&progress_callback](const std::string& line) {
    int firmware_percent = 0;
    if (std::sscanf(line.c_str(),
                    "INFO: Firmware progress: %*d/%*d bytes %d%%",
                    &firmware_percent) == 1) {
      publish(progress_callback,
              55 + std::clamp(firmware_percent, 0, 100) * 35 / 100,
              "Upgrading firmware. The screen may go black...");
    }
  };
  if (!run_command(
          "python3",
          {"-u", script_path.string(), "-b", "/dev/i2c-1", "-f", firmware_path.string(), "-v"},
          180000,
          "\n\n",
          upgrade_line_handler,
          result.error_message)) {
    result.error_status = "Firmware upgrade failed";
    LOG_ERROR("PY32 upgrade failed: {}", result.error_message);
    return result;
  }

  publish(progress_callback, 92, "Verifying upgraded version...");
  result.current_version = device_info::read_py32_firmware_version(true);
  if (result.current_version != K_TARGET_VERSION) {
    result.error_status = "Version verification failed";
    result.error_message =
        "Upgraded version is " +
        (result.current_version.empty() ? std::string{"unavailable"} : result.current_version) +
        ", expected 0xF8";
    return result;
  }

  result.success = true;
  publish(progress_callback, 100, "Upgrade complete (0xF8). Please reboot.");
  LOG_INFO("PY32 upgrade complete: {} -> {}", result.previous_version, result.current_version);
  return result;
}

}  // namespace platform::py32_upgrade
