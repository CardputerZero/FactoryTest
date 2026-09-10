/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "baking_pi_service.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include "logger.h"
#include "power_service.h"

#if defined(__linux__)
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace platform::baking {
namespace {

constexpr int K_CYCLE_SECONDS              = 30 * 60;
constexpr int K_GAP_SECONDS                = 10;
constexpr int K_METRIC_SAMPLE_SECONDS      = 5;
constexpr std::size_t K_METRIC_POINT_COUNT = 120;
constexpr const char* K_LOG_DIRECTORY      = "/tmp/factory_test_baking";
constexpr const char* K_METRICS_PATH       = "/tmp/factory_test_baking/metrics.jsonl";
constexpr const char* K_WORK_DIRECTORY     = "/var/tmp/factory_test_baking";

enum WorkloadIndex : std::size_t {
  CPU_WORKLOAD = 0,
  MEMORY_WORKLOAD,
  STORAGE_WORKLOAD,
  WORKLOAD_COUNT,
};

constexpr std::array<const char*, WORKLOAD_COUNT> K_WORKLOAD_NAMES = {"CPU", "memory", "storage"};

std::int64_t epoch_milliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

bool read_double_file(const std::filesystem::path& path, double& value) {
  std::ifstream file(path);
  return static_cast<bool>(file >> value);
}

std::optional<double> read_soc_temperature() {
#if !defined(__linux__)
  return std::nullopt;
#else
  const std::filesystem::path root("/sys/class/thermal");
  std::error_code error;
  std::optional<double> fallback;
  if (!std::filesystem::exists(root, error)) {
    return std::nullopt;
  }

  for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
    if (error || entry.path().filename().string().find("thermal_zone") != 0) {
      continue;
    }
    std::ifstream type_file(entry.path() / "type");
    std::string type;
    std::getline(type_file, type);
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });

    double raw_temperature = 0.0;
    if (!read_double_file(entry.path() / "temp", raw_temperature)) {
      continue;
    }
    const auto temperature =
        std::abs(raw_temperature) > 1000.0 ? raw_temperature / 1000.0 : raw_temperature;
    if (!fallback) {
      fallback = temperature;
    }
    if (type.find("soc") != std::string::npos || type.find("cpu") != std::string::npos ||
        type.find("package") != std::string::npos) {
      return temperature;
    }
  }
  return fallback;
#endif
}

bool read_cpu_times(std::uint64_t& total, std::uint64_t& idle) {
#if !defined(__linux__)
  (void)total;
  (void)idle;
  return false;
#else
  std::ifstream file("/proc/stat");
  std::string cpu;
  std::array<std::uint64_t, 10> fields{};
  if (!(file >> cpu) || cpu != "cpu") {
    return false;
  }
  std::size_t count = 0;
  while (count < fields.size() && file >> fields[count]) {
    ++count;
  }
  if (count < 4) {
    return false;
  }
  total = 0;
  for (std::size_t i = 0; i < count; ++i) {
    total += fields[i];
  }
  idle = fields[3] + (count > 4 ? fields[4] : 0);
  return true;
#endif
}

std::optional<double> read_memory_usage_percent() {
#if !defined(__linux__)
  return std::nullopt;
#else
  std::ifstream file("/proc/meminfo");
  std::string key;
  std::uint64_t value = 0;
  std::string unit;
  std::uint64_t total     = 0;
  std::uint64_t available = 0;
  while (file >> key >> value >> unit) {
    if (key == "MemTotal:") {
      total = value;
    } else if (key == "MemAvailable:") {
      available = value;
    }
  }
  if (total == 0 || available > total) {
    return std::nullopt;
  }
  return static_cast<double>(total - available) * 100.0 / static_cast<double>(total);
#endif
}

struct DiskStats {
  std::uint64_t read_sectors{0};
  std::uint64_t write_sectors{0};
  std::string device_name{};
};

bool read_root_disk_stats(DiskStats& stats) {
#if !defined(__linux__)
  (void)stats;
  return false;
#else
  struct stat root_stat{};
  if (::stat("/", &root_stat) != 0) {
    return false;
  }
  const auto root_major = static_cast<unsigned int>(major(root_stat.st_dev));
  const auto root_minor = static_cast<unsigned int>(minor(root_stat.st_dev));

  std::ifstream file("/proc/diskstats");
  std::string line;
  while (std::getline(file, line)) {
    std::istringstream input(line);
    unsigned int device_major = 0;
    unsigned int device_minor = 0;
    std::string device_name;
    std::uint64_t reads           = 0;
    std::uint64_t reads_merged    = 0;
    std::uint64_t sectors_read    = 0;
    std::uint64_t read_ms         = 0;
    std::uint64_t writes          = 0;
    std::uint64_t writes_merged   = 0;
    std::uint64_t sectors_written = 0;
    std::uint64_t write_ms        = 0;
    std::uint64_t in_flight       = 0;
    std::uint64_t io_ms           = 0;
    if (!(input >> device_major >> device_minor >> device_name >> reads >> reads_merged >>
          sectors_read >> read_ms >> writes >> writes_merged >> sectors_written >> write_ms >>
          in_flight >> io_ms)) {
      continue;
    }
    if (device_major == root_major && device_minor == root_minor) {
      stats.read_sectors  = sectors_read;
      stats.write_sectors = sectors_written;
      stats.device_name   = std::move(device_name);
      return true;
    }
  }
  return false;
#endif
}

std::optional<double> read_root_storage_usage_percent() {
#if !defined(__linux__)
  return std::nullopt;
#else
  struct statvfs stats{};
  if (statvfs("/", &stats) != 0 || stats.f_blocks == 0) {
    return std::nullopt;
  }
  const auto used_blocks = stats.f_blocks > stats.f_bavail ? stats.f_blocks - stats.f_bavail : 0;
  return static_cast<double>(used_blocks) * 100.0 / static_cast<double>(stats.f_blocks);
#endif
}

void write_optional_number(std::ostream& output,
                           const std::optional<double>& value,
                           int precision = 3) {
  if (!value || !std::isfinite(*value)) {
    output << "null";
    return;
  }
  output << std::fixed << std::setprecision(precision) << *value;
}

std::string escape_json_string(const std::string& value) {
  std::string result;
  result.reserve(value.size());
  for (const auto ch : value) {
    switch (ch) {
      case '\\':
        result += "\\\\";
        break;
      case '"':
        result += "\\\"";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        result += ch;
        break;
    }
  }
  return result;
}

#if defined(__linux__)
struct Workload {
  const char* log_name;
  const char* executable;
  std::vector<std::string> args;
};

bool executable_exists(const char* executable) {
  if (!executable || !*executable) {
    return false;
  }
  const char* path  = std::getenv("PATH");
  std::string paths = path && *path ? path : "/usr/local/bin:/usr/bin:/bin";
  std::size_t start = 0;
  while (start <= paths.size()) {
    const auto end = paths.find(':', start);
    const auto directory =
        paths.substr(start, end == std::string::npos ? std::string::npos : end - start);
    const auto candidate = (directory.empty() ? std::string(".") : directory) + "/" + executable;
    if (access(candidate.c_str(), X_OK) == 0) {
      return true;
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return false;
}

std::vector<Workload> make_workloads(const std::string& log_directory) {
  const std::string taskset = executable_exists("taskset") ? "taskset" : "";
  const auto with_affinity  = [&taskset](std::vector<std::string> args) {
    if (taskset.empty()) {
      return args;
    }
    std::vector<std::string> result{"-c", "0-2"};
    result.push_back(args.front());
    result.insert(result.end(), args.begin() + 1, args.end());
    return result;
  };

  const std::string work_file   = std::string(K_WORK_DIRECTORY) + "/mmc-rw.bin";
  const std::string memory_yaml = log_directory + "/memory.yaml";
  return {
      {"sysbench",
       taskset.empty() ? "sysbench" : "taskset",
       with_affinity({"sysbench",
                      "cpu",
                      "--threads=3",
                      "--time=1800",
                      "--cpu-max-prime=20000",
                      "--report-interval=5",
                      "run"})},
      {"stress-ng",
       taskset.empty() ? "stress-ng" : "taskset",
       with_affinity({"stress-ng",
                      "--vm",
                      "1",
                      "--vm-bytes",
                      "160M",
                      "--vm-keep",
                      "--vm-populate",
                      "--memcpy",
                      "2",
                      "--timeout",
                      "30m",
                      "--metrics-brief",
                      "--yaml",
                      memory_yaml})},
      {"fio",
       taskset.empty() ? "fio" : "taskset",
       with_affinity({"fio",
                      "--name=mmc_randrw",
                      "--ioengine=libaio",
                      "--iodepth=8",
                      "--rw=randrw",
                      "--rwmixread=50",
                      "--bs=4k",
                      "--direct=1",
                      "--size=64M",
                      "--runtime=1800",
                      "--time_based",
                      "--numjobs=1",
                      "--status-interval=5",
                      "--group_reporting",
                      "--filename=" + work_file})},
  };
}

bool spawn_workload(const Workload& workload,
                    const std::string& log_path,
                    bool append_log,
                    int& pid) {
  pid               = -1;
  const int flags   = O_WRONLY | O_CREAT | O_CLOEXEC | (append_log ? O_APPEND : O_TRUNC);
  const auto log_fd = open(log_path.c_str(), flags, 0644);
  if (log_fd < 0) {
    LOG_ERROR("[BAKING] failed to open {}: {}", log_path, std::strerror(errno));
    return false;
  }

  std::vector<char*> argv;
  argv.reserve(workload.args.size() + 2);
  argv.push_back(const_cast<char*>(workload.executable));
  for (const auto& arg : workload.args) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }
  argv.push_back(nullptr);

  const auto child = fork();
  if (child < 0) {
    LOG_ERROR("[BAKING] failed to fork {}: {}", workload.log_name, std::strerror(errno));
    close(log_fd);
    return false;
  }
  if (child == 0) {
    setpgid(0, 0);
    setpriority(PRIO_PROCESS, 0, 10);
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
    close(log_fd);
    execvp(workload.executable, argv.data());
    _exit(127);
  }

  close(log_fd);
  setpgid(child, child);
  pid = static_cast<int>(child);
  return true;
}
#endif

}  // namespace

BakingPiService::~BakingPiService() { stop(); }

bool BakingPiService::start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    LOG_DEBUG("[BAKING] start ignored: service already running");
    return false;
  }

  stop_requested_.store(false);
  previous_cpu_valid_  = false;
  previous_disk_valid_ = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_              = BakingPiSnapshot{};
    snapshot_.running      = true;
    snapshot_.metrics_path = K_METRICS_PATH;
    child_processes_.clear();
    metric_history_.clear();
  }
  worker_ = std::thread(&BakingPiService::worker_loop_, this);
  LOG_INFO("[BAKING] service started: 30-minute CPU/memory/MMC cycles");
  return true;
}

void BakingPiService::stop() {
  if (!running_.load() && !worker_.joinable()) {
    return;
  }

  LOG_INFO("[BAKING] stop requested; terminating pressure process groups");
  stop_requested_.store(true);
#if defined(__linux__)
  stop_children_(SIGTERM);
#endif
  if (worker_.joinable()) {
    worker_.join();
  }
#if defined(__linux__)
  stop_children_(SIGKILL);
  wait_children_(1);
#endif
  running_.store(false);
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.running                  = false;
  snapshot_.in_gap                   = false;
  snapshot_.cpu_workload_running     = false;
  snapshot_.memory_workload_running  = false;
  snapshot_.storage_workload_running = false;
  LOG_INFO("[BAKING] service stopped; all pressure processes reaped");
}

BakingPiSnapshot BakingPiService::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshot_;
}

std::vector<BakingMetricSample> BakingPiService::metric_history() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return {metric_history_.begin(), metric_history_.end()};
}

void BakingPiService::worker_loop_() {
#if !defined(__linux__)
  set_error_("Baking pressure test is only available on Linux");
  running_.store(false);
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.running = false;
  return;
#else
  std::error_code error;
  std::filesystem::create_directories(K_LOG_DIRECTORY, error);
  if (error) {
    set_error_("Failed to create baking log directory");
    LOG_ERROR("[BAKING] failed to create log directory {}: {}", K_LOG_DIRECTORY, error.message());
  }
  error.clear();
  std::filesystem::create_directories(K_WORK_DIRECTORY, error);
  if (error) {
    set_error_("Failed to create baking work directory");
    LOG_ERROR("[BAKING] failed to create work directory {}: {}", K_WORK_DIRECTORY, error.message());
  }
  {
    std::ofstream metrics_file(K_METRICS_PATH, std::ios::trunc);
    if (!metrics_file) {
      set_error_("Failed to open metrics JSONL");
    }
  }

  std::uint64_t cycle = 0;
  while (!stop_requested_.load()) {
    ++cycle;
    const auto log_directory = std::string(K_LOG_DIRECTORY) + "/cycle-" + std::to_string(cycle);
    error.clear();
    std::filesystem::create_directories(log_directory, error);
    if (error) {
      set_error_("Failed to create cycle log directory");
      LOG_ERROR("[BAKING] failed to create cycle log directory {}: {}",
                log_directory,
                error.message());
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.cycle                    = cycle;
      snapshot_.elapsed_seconds          = 0;
      snapshot_.in_gap                   = false;
      snapshot_.cpu_workload_running     = false;
      snapshot_.memory_workload_running  = false;
      snapshot_.storage_workload_running = false;
    }
    LOG_INFO("[BAKING] cycle {} starting: CPU cores 0-2, LVGL reserved on core 3", cycle);

    if (!launch_workloads_(cycle, log_directory)) {
      set_error_("One or more workloads failed to start");
    }

    const auto started_at      = std::chrono::steady_clock::now();
    auto next_metric_sample_at = started_at;
    while (!stop_requested_.load()) {
      const auto now     = std::chrono::steady_clock::now();
      const auto elapsed = static_cast<int>(
          std::chrono::duration_cast<std::chrono::seconds>(now - started_at).count());
      {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.elapsed_seconds = std::min(K_CYCLE_SECONDS, std::max(0, elapsed));
      }

      const auto exited_workloads = reap_children_();
      if (!stop_requested_.load() && elapsed < K_CYCLE_SECONDS) {
        for (const auto& workload_exit : exited_workloads) {
          restart_workload_(workload_exit, cycle, log_directory);
        }
        if (exited_workloads.empty()) {
          retry_missing_workloads_(cycle, log_directory);
        }
      }

      if (now >= next_metric_sample_at) {
        append_metric_sample_(collect_metric_sample_(cycle, elapsed));
        next_metric_sample_at = now + std::chrono::seconds(K_METRIC_SAMPLE_SECONDS);
      }
      if (elapsed >= K_CYCLE_SECONDS) {
        break;
      }
      wait_interruptible_(1);
    }

    stop_children_(SIGTERM);
    wait_children_(2);
    stop_children_(SIGKILL);
    wait_children_(1);
    if (stop_requested_.load()) {
      break;
    }
    LOG_INFO("[BAKING] cycle {} complete; pausing {} seconds before restart", cycle, K_GAP_SECONDS);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.in_gap          = true;
      snapshot_.elapsed_seconds = K_CYCLE_SECONDS;
    }
    wait_interruptible_(K_GAP_SECONDS);
  }

  stop_children_(SIGKILL);
  wait_children_(1);
  std::error_code cleanup_error;
  std::filesystem::remove(std::string(K_WORK_DIRECTORY) + "/mmc-rw.bin", cleanup_error);
  if (cleanup_error) {
    LOG_DEBUG("[BAKING] failed to remove workload file: {}", cleanup_error.message());
  }
  running_.store(false);
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.running = false;
#endif
}

bool BakingPiService::launch_workloads_(std::uint64_t cycle, const std::string& log_directory) {
  bool all_started = true;
  for (std::size_t index = 0; index < WORKLOAD_COUNT; ++index) {
    all_started = launch_workload_(index, cycle, log_directory, false) && all_started;
  }
  return all_started;
}

bool BakingPiService::launch_workload_(std::size_t workload_index,
                                       std::uint64_t cycle,
                                       const std::string& log_directory,
                                       bool append_log) {
  if (stop_requested_.load()) {
    return false;
  }
#if !defined(__linux__)
  (void)workload_index;
  (void)cycle;
  (void)log_directory;
  (void)append_log;
  return false;
#else
  const auto workloads = make_workloads(log_directory);
  if (workload_index >= workloads.size()) {
    return false;
  }
  const auto& workload = workloads[workload_index];
  int pid              = -1;
  const auto log_path  = log_directory + "/" + workload.log_name + ".log";
  if (!spawn_workload(workload, log_path, append_log, pid)) {
    set_workload_running_(workload_index, false);
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    child_processes_.push_back({pid, workload_index});
  }
  set_workload_running_(workload_index, true);
  LOG_INFO("[BAKING] cycle {} started {} (pid={}, log={})",
           cycle,
           workload.log_name,
           pid,
           log_path);
  return true;
#endif
}

void BakingPiService::restart_workload_(const WorkloadExit& workload_exit,
                                        std::uint64_t cycle,
                                        const std::string& log_directory) {
  if (workload_exit.workload_index >= K_WORKLOAD_NAMES.size()) {
    return;
  }
  const auto* name = K_WORKLOAD_NAMES[workload_exit.workload_index];
  LOG_WARN("[BAKING] {} workload exited early (status={}); restarting", name, workload_exit.status);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ++snapshot_.restart_count;
  }
  if (launch_workload_(workload_exit.workload_index, cycle, log_directory, true)) {
    set_error_(std::string(name) + " workload exited; restarted");
  } else {
    set_error_(std::string(name) + " workload restart failed");
  }
}

void BakingPiService::retry_missing_workloads_(std::uint64_t cycle,
                                               const std::string& log_directory) {
  for (std::size_t index = 0; index < WORKLOAD_COUNT && !stop_requested_.load(); ++index) {
    if (workload_running_(index)) {
      continue;
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      ++snapshot_.restart_count;
    }
    const auto* name = K_WORKLOAD_NAMES[index];
    if (launch_workload_(index, cycle, log_directory, true)) {
      set_error_(std::string(name) + " workload recovered");
    } else {
      set_error_(std::string(name) + " workload retry failed");
    }
  }
}

void BakingPiService::stop_children_(int signal) {
#if defined(__linux__)
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& child : child_processes_) {
    if (child.pid > 0) {
      if (kill(-child.pid, signal) != 0 && errno != ESRCH) {
        LOG_WARN("[BAKING] failed to signal process group {}: {}", child.pid, std::strerror(errno));
      }
      kill(child.pid, signal);
    }
  }
#else
  (void)signal;
#endif
}

void BakingPiService::wait_children_(int timeout_seconds) {
#if defined(__linux__)
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
  while (true) {
    reap_children_();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (child_processes_.empty()) {
        return;
      }
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
#else
  (void)timeout_seconds;
#endif
}

std::vector<BakingPiService::WorkloadExit> BakingPiService::reap_children_() {
  std::vector<WorkloadExit> exited;
#if defined(__linux__)
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = child_processes_.begin();
  while (it != child_processes_.end()) {
    int status        = 0;
    const auto result = waitpid(static_cast<pid_t>(it->pid), &status, WNOHANG);
    if (result == 0) {
      ++it;
      continue;
    }
    if (result == it->pid) {
      if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        LOG_INFO("[BAKING] workload pid {} exited successfully", it->pid);
      } else {
        LOG_WARN("[BAKING] workload pid {} exited abnormally (status={})", it->pid, status);
      }
      exited.push_back({it->workload_index, status});
      switch (it->workload_index) {
        case CPU_WORKLOAD:
          snapshot_.cpu_workload_running = false;
          break;
        case MEMORY_WORKLOAD:
          snapshot_.memory_workload_running = false;
          break;
        case STORAGE_WORKLOAD:
          snapshot_.storage_workload_running = false;
          break;
        default:
          break;
      }
      it = child_processes_.erase(it);
      continue;
    }
    if (result < 0 && errno == ECHILD) {
      exited.push_back({it->workload_index, status});
      switch (it->workload_index) {
        case CPU_WORKLOAD:
          snapshot_.cpu_workload_running = false;
          break;
        case MEMORY_WORKLOAD:
          snapshot_.memory_workload_running = false;
          break;
        case STORAGE_WORKLOAD:
          snapshot_.storage_workload_running = false;
          break;
        default:
          break;
      }
      it = child_processes_.erase(it);
      continue;
    }
    ++it;
  }
#endif
  return exited;
}

bool BakingPiService::wait_interruptible_(int seconds) {
  for (int second = 0; second < seconds && !stop_requested_.load(); ++second) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return !stop_requested_.load();
}

BakingMetricSample BakingPiService::collect_metric_sample_(std::uint64_t cycle,
                                                           int elapsed_seconds) {
  BakingMetricSample sample;
  sample.timestamp_ms      = epoch_milliseconds();
  sample.cycle             = cycle;
  sample.elapsed_seconds   = std::max(0, elapsed_seconds);
  sample.soc_temperature_c = read_soc_temperature();

  power::PowerSupplyInfo power_info;
  std::string power_error;
  if (power::read_battery_info(power_info, power_error)) {
    if (power_info.temp_decic > -100000) {
      sample.battery_temperature_c = static_cast<double>(power_info.temp_decic) / 10.0;
    }
    const auto voltage = power_info.voltage_instant_uv != -1 ? power_info.voltage_instant_uv
                                                             : power_info.voltage_now_uv;
    const auto current = power_info.current_instant_ua != -1 ? power_info.current_instant_ua
                                                             : power_info.current_now_ua;
    if (voltage >= 0) {
      sample.battery_voltage_v = static_cast<double>(voltage) / 1000000.0;
    }
    if (current != -1) {
      sample.battery_current_a = static_cast<double>(current) / 1000000.0;
    }
  }

  std::uint64_t cpu_total = 0;
  std::uint64_t cpu_idle  = 0;
  if (read_cpu_times(cpu_total, cpu_idle)) {
    if (previous_cpu_valid_ && cpu_total > previous_cpu_total_) {
      const auto total_delta = cpu_total - previous_cpu_total_;
      const auto idle_delta  = cpu_idle >= previous_cpu_idle_ ? cpu_idle - previous_cpu_idle_ : 0;
      sample.cpu_usage_percent =
          static_cast<double>(total_delta - std::min(total_delta, idle_delta)) * 100.0 /
          static_cast<double>(total_delta);
    }
    previous_cpu_total_ = cpu_total;
    previous_cpu_idle_  = cpu_idle;
    previous_cpu_valid_ = true;
  }
  sample.memory_usage_percent  = read_memory_usage_percent();
  sample.storage_usage_percent = read_root_storage_usage_percent();

  DiskStats disk_stats;
  if (read_root_disk_stats(disk_stats)) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.storage_device = disk_stats.device_name;
    }
    if (previous_disk_valid_ && sample.timestamp_ms > previous_disk_timestamp_ms_) {
      const auto elapsed_ms  = sample.timestamp_ms - previous_disk_timestamp_ms_;
      const auto read_delta  = disk_stats.read_sectors >= previous_disk_read_sectors_
                                   ? disk_stats.read_sectors - previous_disk_read_sectors_
                                   : 0;
      const auto write_delta = disk_stats.write_sectors >= previous_disk_write_sectors_
                                   ? disk_stats.write_sectors - previous_disk_write_sectors_
                                   : 0;
      // Linux block statistics report 512-byte sectors. Convert to KiB/s.
      sample.storage_read_kib_per_second =
          static_cast<double>(read_delta) * 500.0 / static_cast<double>(elapsed_ms);
      sample.storage_write_kib_per_second =
          static_cast<double>(write_delta) * 500.0 / static_cast<double>(elapsed_ms);
    }
    previous_disk_read_sectors_  = disk_stats.read_sectors;
    previous_disk_write_sectors_ = disk_stats.write_sectors;
    previous_disk_timestamp_ms_  = sample.timestamp_ms;
    previous_disk_valid_         = true;
  }
  return sample;
}

bool BakingPiService::append_metric_sample_(BakingMetricSample sample) {
  BakingPiSnapshot snapshot;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    sample.cpu_workload_running     = snapshot_.cpu_workload_running;
    sample.memory_workload_running  = snapshot_.memory_workload_running;
    sample.storage_workload_running = snapshot_.storage_workload_running;
    metric_history_.push_back(sample);
    while (metric_history_.size() > K_METRIC_POINT_COUNT) {
      metric_history_.pop_front();
    }
    snapshot = snapshot_;
  }

  std::ofstream output(K_METRICS_PATH, std::ios::app);
  if (!output) {
    set_error_("Failed to append metrics JSONL");
    return false;
  }
  output << "{\"timestamp_ms\":" << sample.timestamp_ms << ",\"cycle\":" << sample.cycle
         << ",\"elapsed_s\":" << sample.elapsed_seconds << ",\"soc_temp_c\":";
  write_optional_number(output, sample.soc_temperature_c);
  output << ",\"battery_temp_c\":";
  write_optional_number(output, sample.battery_temperature_c);
  output << ",\"voltage_v\":";
  write_optional_number(output, sample.battery_voltage_v);
  output << ",\"current_a\":";
  write_optional_number(output, sample.battery_current_a);
  output << ",\"cpu_percent\":";
  write_optional_number(output, sample.cpu_usage_percent);
  output << ",\"memory_percent\":";
  write_optional_number(output, sample.memory_usage_percent);
  output << ",\"storage_percent\":";
  write_optional_number(output, sample.storage_usage_percent);
  output << ",\"storage_read_kib_s\":";
  write_optional_number(output, sample.storage_read_kib_per_second);
  output << ",\"storage_write_kib_s\":";
  write_optional_number(output, sample.storage_write_kib_per_second);
  output << ",\"storage_device\":\"" << escape_json_string(snapshot.storage_device)
         << "\",\"workloads\":{\"cpu\":" << (sample.cpu_workload_running ? "true" : "false")
         << ",\"memory\":" << (sample.memory_workload_running ? "true" : "false")
         << ",\"storage\":" << (sample.storage_workload_running ? "true" : "false")
         << "},\"restart_count\":" << snapshot.restart_count << ",\"error\":\""
         << escape_json_string(snapshot.error_message) << "\"}\n";
  if (!output) {
    set_error_("Failed to write metrics JSONL");
    return false;
  }
  return true;
}

void BakingPiService::set_error_(std::string message) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.error_message = std::move(message);
}

void BakingPiService::set_workload_running_(std::size_t workload_index, bool running) {
  std::lock_guard<std::mutex> lock(mutex_);
  switch (workload_index) {
    case CPU_WORKLOAD:
      snapshot_.cpu_workload_running = running;
      break;
    case MEMORY_WORKLOAD:
      snapshot_.memory_workload_running = running;
      break;
    case STORAGE_WORKLOAD:
      snapshot_.storage_workload_running = running;
      break;
    default:
      break;
  }
}

bool BakingPiService::workload_running_(std::size_t workload_index) const {
  std::lock_guard<std::mutex> lock(mutex_);
  switch (workload_index) {
    case CPU_WORKLOAD:
      return snapshot_.cpu_workload_running;
    case MEMORY_WORKLOAD:
      return snapshot_.memory_workload_running;
    case STORAGE_WORKLOAD:
      return snapshot_.storage_workload_running;
    default:
      return false;
  }
}

}  // namespace platform::baking
