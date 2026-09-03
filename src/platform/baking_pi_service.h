/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace platform::baking {

struct BakingMetricSample {
  std::int64_t timestamp_ms{0};
  std::uint64_t cycle{0};
  int elapsed_seconds{0};
  std::optional<double> soc_temperature_c{};
  std::optional<double> battery_temperature_c{};
  std::optional<double> battery_voltage_v{};
  std::optional<double> battery_current_a{};
  std::optional<double> cpu_usage_percent{};
  std::optional<double> memory_usage_percent{};
  std::optional<double> storage_usage_percent{};
  std::optional<double> storage_read_kib_per_second{};
  std::optional<double> storage_write_kib_per_second{};
  bool cpu_workload_running{false};
  bool memory_workload_running{false};
  bool storage_workload_running{false};
};

struct BakingPiSnapshot {
  bool running{false};
  bool in_gap{false};
  std::uint64_t cycle{0};
  int elapsed_seconds{0};
  std::uint64_t restart_count{0};
  bool cpu_workload_running{false};
  bool memory_workload_running{false};
  bool storage_workload_running{false};
  std::string storage_device{};
  std::string metrics_path{};
  std::string error_message{};
};

// Owns the long-running pressure processes and the telemetry collected while
// the manual "Fire in the Pi" test is active.
class BakingPiService {
 public:
  BakingPiService() = default;
  ~BakingPiService();

  BakingPiService(const BakingPiService&)            = delete;
  BakingPiService& operator=(const BakingPiService&) = delete;

  bool start();
  void stop();
  bool running() const { return running_.load(); }
  BakingPiSnapshot snapshot() const;
  std::vector<BakingMetricSample> metric_history() const;

 private:
  struct ChildProcess {
    int pid{-1};
    std::size_t workload_index{0};
  };

  struct WorkloadExit {
    std::size_t workload_index{0};
    int status{0};
  };

  void worker_loop_();
  bool launch_workloads_(std::uint64_t cycle, const std::string& log_directory);
  bool launch_workload_(std::size_t workload_index,
                        std::uint64_t cycle,
                        const std::string& log_directory,
                        bool append_log);
  void restart_workload_(const WorkloadExit& workload_exit,
                         std::uint64_t cycle,
                         const std::string& log_directory);
  void retry_missing_workloads_(std::uint64_t cycle, const std::string& log_directory);
  void stop_children_(int signal);
  std::vector<WorkloadExit> reap_children_();
  void wait_children_(int timeout_seconds);
  bool wait_interruptible_(int seconds);
  BakingMetricSample collect_metric_sample_(std::uint64_t cycle, int elapsed_seconds);
  bool append_metric_sample_(BakingMetricSample sample);
  void set_error_(std::string message);
  void set_workload_running_(std::size_t workload_index, bool running);
  bool workload_running_(std::size_t workload_index) const;

  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> running_{false};
  mutable std::mutex mutex_;
  std::vector<ChildProcess> child_processes_;
  std::deque<BakingMetricSample> metric_history_;
  BakingPiSnapshot snapshot_{};
  std::thread worker_;
  std::uint64_t previous_cpu_total_{0};
  std::uint64_t previous_cpu_idle_{0};
  bool previous_cpu_valid_{false};
  std::uint64_t previous_disk_read_sectors_{0};
  std::uint64_t previous_disk_write_sectors_{0};
  std::int64_t previous_disk_timestamp_ms_{0};
  bool previous_disk_valid_{false};
};

}  // namespace platform::baking
