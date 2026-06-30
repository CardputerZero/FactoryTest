/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "process_service.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>
#include <utility>

#include "logger.h"
#include "serialization.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <crt_externs.h>
#endif
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
extern char** environ;
#endif

namespace platform::process {
namespace {

constexpr std::size_t K_READ_BUFFER_SIZE = 4096;

void append_output(std::string& output,
                   const char* data,
                   std::size_t size,
                   const OutputHandler& handler,
                   const OutputLineHandler& line_handler,
                   std::string& pending_line,
                   const ProcessOptions& options,
                   ProcessResult& result) {
  if (handler) {
    handler(data, size);
  }
  if (line_handler) {
    pending_line.append(data, size);
    std::size_t newline = 0;
    while ((newline = pending_line.find_first_of("\r\n")) != std::string::npos) {
      const auto delimiter    = pending_line[newline];
      auto line               = pending_line.substr(0, newline);
      std::size_t erase_count = 1;
      if (delimiter == '\r' && newline + 1 < pending_line.size() &&
          pending_line[newline + 1] == '\n') {
        erase_count = 2;
      }
      pending_line.erase(0, newline + erase_count);
      line_handler(line);
    }
  }

  const auto available =
      output.size() < options.max_output_bytes ? options.max_output_bytes - output.size() : 0U;
  const auto copy_size = std::min(size, available);
  if (copy_size > 0) {
    output.append(data, copy_size);
  }
  if (copy_size < size) {
    result.output_truncated = true;
  }
}

void flush_pending_output_line(std::string& pending_line, const OutputLineHandler& line_handler) {
  if (!line_handler || pending_line.empty()) {
    return;
  }
  if (!pending_line.empty() && pending_line.back() == '\r') {
    pending_line.pop_back();
  }
  line_handler(pending_line);
  pending_line.clear();
}

#if !defined(_WIN32)
#if defined(__APPLE__)
char** process_environment() { return *_NSGetEnviron(); }
#else
char** process_environment() { return ::environ; }
#endif

struct FileActionsOwner {
  posix_spawn_file_actions_t actions{};
  bool initialized{false};

  FileActionsOwner() { initialized = posix_spawn_file_actions_init(&actions) == 0; }

  ~FileActionsOwner() {
    if (initialized) {
      posix_spawn_file_actions_destroy(&actions);
    }
  }
};

void close_fd(int& fd) {
  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}

bool make_pipe(int fds[2], std::string& error_message) {
  fds[0] = -1;
  fds[1] = -1;
  if (pipe(fds) != 0) {
    error_message = std::strerror(errno);
    return false;
  }
  return true;
}

void set_non_blocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags >= 0) {
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }
}

void read_available(int fd,
                    bool& open,
                    std::string& output,
                    const OutputHandler& handler,
                    const OutputLineHandler& line_handler,
                    std::string& pending_line,
                    const ProcessOptions& options,
                    ProcessResult& result) {
  char buffer[K_READ_BUFFER_SIZE];
  while (open) {
    const auto count = read(fd, buffer, sizeof(buffer));
    if (count > 0) {
      append_output(output,
                    buffer,
                    static_cast<std::size_t>(count),
                    handler,
                    line_handler,
                    pending_line,
                    options,
                    result);
      continue;
    }
    if (count == 0) {
      open = false;
      return;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return;
    }
    open = false;
    return;
  }
}

void fill_exit_status(int status, ProcessResult& result) {
  if (WIFEXITED(status)) {
    result.exited    = true;
    result.exit_code = WEXITSTATUS(status);
    return;
  }
  if (WIFSIGNALED(status)) {
    result.exited      = false;
    result.term_signal = WTERMSIG(status);
  }
}

ProcessResult run_spawned_command(const std::string& executable,
                                  const std::vector<std::string>& args,
                                  const ProcessOptions& options) {
  ProcessResult result;
  if (executable.empty()) {
    result.error_message = "empty executable";
    return result;
  }

  int stdout_pipe[2] = {-1, -1};
  int stderr_pipe[2] = {-1, -1};
  if (!make_pipe(stdout_pipe, result.error_message) ||
      !make_pipe(stderr_pipe, result.error_message)) {
    close_fd(stdout_pipe[0]);
    close_fd(stdout_pipe[1]);
    close_fd(stderr_pipe[0]);
    close_fd(stderr_pipe[1]);
    return result;
  }

  FileActionsOwner file_actions;
  if (!file_actions.initialized) {
    result.error_message = "failed to initialize process file actions";
    close_fd(stdout_pipe[0]);
    close_fd(stdout_pipe[1]);
    close_fd(stderr_pipe[0]);
    close_fd(stderr_pipe[1]);
    return result;
  }

  posix_spawn_file_actions_adddup2(&file_actions.actions, stdout_pipe[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&file_actions.actions, stderr_pipe[1], STDERR_FILENO);
  posix_spawn_file_actions_addclose(&file_actions.actions, stdout_pipe[0]);
  posix_spawn_file_actions_addclose(&file_actions.actions, stderr_pipe[0]);
  posix_spawn_file_actions_addclose(&file_actions.actions, stdout_pipe[1]);
  posix_spawn_file_actions_addclose(&file_actions.actions, stderr_pipe[1]);

  std::vector<std::string> argv_storage;
  argv_storage.reserve(args.size() + 1);
  argv_storage.push_back(executable);
  argv_storage.insert(argv_storage.end(), args.begin(), args.end());

  std::vector<char*> argv;
  argv.reserve(argv_storage.size() + 1);
  for (auto& value : argv_storage) {
    argv.push_back(value.data());
  }
  argv.push_back(nullptr);

  pid_t pid         = -1;
  const int spawned = posix_spawnp(&pid,
                                   executable.c_str(),
                                   &file_actions.actions,
                                   nullptr,
                                   argv.data(),
                                   process_environment());

  close_fd(stdout_pipe[1]);
  close_fd(stderr_pipe[1]);
  if (spawned != 0) {
    result.error_message = std::strerror(spawned);
    close_fd(stdout_pipe[0]);
    close_fd(stderr_pipe[0]);
    return result;
  }

  LOG_TRACE("process spawned: executable='{}' pid={}", executable, static_cast<int>(pid));
  set_non_blocking(stdout_pipe[0]);
  set_non_blocking(stderr_pipe[0]);

  bool stdout_open = true;
  bool stderr_open = true;
  bool child_done  = false;
  int child_status = 0;
  std::string stdout_pending_line;
  std::string stderr_pending_line;
  const auto start = std::chrono::steady_clock::now();

  while (stdout_open || stderr_open) {
    if (!child_done) {
      const auto wait_result = waitpid(pid, &child_status, WNOHANG);
      if (wait_result == pid) {
        child_done = true;
      }
    }

    if (!child_done && options.timeout_ms > 0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();
      if (elapsed >= options.timeout_ms) {
        result.timed_out = true;
        kill(pid, SIGKILL);
        waitpid(pid, &child_status, 0);
        child_done = true;
      }
    }

    pollfd fds[2] = {};
    nfds_t count  = 0;
    if (stdout_open) {
      fds[count++] = pollfd{stdout_pipe[0], static_cast<short>(POLLIN | POLLHUP | POLLERR), 0};
    }
    if (stderr_open) {
      fds[count++] = pollfd{stderr_pipe[0], static_cast<short>(POLLIN | POLLHUP | POLLERR), 0};
    }

    const int poll_result = poll(fds, count, 50);
    if (poll_result < 0 && errno != EINTR) {
      result.error_message = std::strerror(errno);
      break;
    }

    nfds_t index = 0;
    if (stdout_open) {
      if (poll_result > 0 && (fds[index].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
        read_available(stdout_pipe[0],
                       stdout_open,
                       result.stdout_text,
                       options.stdout_handler,
                       options.stdout_line_handler,
                       stdout_pending_line,
                       options,
                       result);
      }
      ++index;
    }
    if (stderr_open) {
      if (poll_result > 0 && (fds[index].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
        read_available(stderr_pipe[0],
                       stderr_open,
                       result.stderr_text,
                       options.stderr_handler,
                       options.stderr_line_handler,
                       stderr_pending_line,
                       options,
                       result);
      }
    }

    if (child_done) {
      read_available(stdout_pipe[0],
                     stdout_open,
                     result.stdout_text,
                     options.stdout_handler,
                     options.stdout_line_handler,
                     stdout_pending_line,
                     options,
                     result);
      read_available(stderr_pipe[0],
                     stderr_open,
                     result.stderr_text,
                     options.stderr_handler,
                     options.stderr_line_handler,
                     stderr_pending_line,
                     options,
                     result);
    }
  }

  close_fd(stdout_pipe[0]);
  close_fd(stderr_pipe[0]);
  flush_pending_output_line(stdout_pending_line, options.stdout_line_handler);
  flush_pending_output_line(stderr_pending_line, options.stderr_line_handler);

  if (!child_done) {
    waitpid(pid, &child_status, 0);
  }
  fill_exit_status(child_status, result);
  LOG_TRACE("process finished: executable='{}' exit_code={} signal={} timed_out={}",
            executable,
            result.exit_code,
            result.term_signal,
            result.timed_out);
  return result;
}
#else
std::string quote_windows_arg(const std::string& arg) {
  if (arg.empty() || arg.find_first_of(" \t\n\v\"") != std::string::npos) {
    std::string quoted = "\"";
    for (const auto ch : arg) {
      if (ch == '"') {
        quoted += "\\\"";
      } else {
        quoted += ch;
      }
    }
    quoted += "\"";
    return quoted;
  }
  return arg;
}

ProcessResult run_spawned_command(const std::string& executable,
                                  const std::vector<std::string>& args,
                                  const ProcessOptions& options) {
  ProcessResult result;
  if (executable.empty()) {
    result.error_message = "empty executable";
    return result;
  }

  std::string command = quote_windows_arg(executable);
  for (const auto& arg : args) {
    command += " ";
    command += quote_windows_arg(arg);
  }

  SECURITY_ATTRIBUTES attributes{};
  attributes.nLength        = sizeof(attributes);
  attributes.bInheritHandle = TRUE;

  HANDLE stdout_read  = nullptr;
  HANDLE stdout_write = nullptr;
  HANDLE stderr_read  = nullptr;
  HANDLE stderr_write = nullptr;
  if (!CreatePipe(&stdout_read, &stdout_write, &attributes, 0) ||
      !CreatePipe(&stderr_read, &stderr_write, &attributes, 0)) {
    result.error_message = "failed to create process pipes";
    return result;
  }
  SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOA startup{};
  startup.cb         = sizeof(startup);
  startup.dwFlags    = STARTF_USESTDHANDLES;
  startup.hStdOutput = stdout_write;
  startup.hStdError  = stderr_write;
  startup.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

  PROCESS_INFORMATION process{};
  std::vector<char> command_line(command.begin(), command.end());
  command_line.push_back('\0');
  const BOOL created = CreateProcessA(nullptr,
                                      command_line.data(),
                                      nullptr,
                                      nullptr,
                                      TRUE,
                                      CREATE_NO_WINDOW,
                                      nullptr,
                                      nullptr,
                                      &startup,
                                      &process);
  CloseHandle(stdout_write);
  CloseHandle(stderr_write);

  if (!created) {
    result.error_message = "failed to create process";
    CloseHandle(stdout_read);
    CloseHandle(stderr_read);
    return result;
  }

  std::mutex output_mutex;
  std::string stdout_pending_line;
  std::string stderr_pending_line;
  auto read_pipe = [&](HANDLE handle,
                       std::string& output,
                       const OutputHandler& handler,
                       const OutputLineHandler& line_handler,
                       std::string& pending_line) {
    char buffer[K_READ_BUFFER_SIZE];
    DWORD read_count = 0;
    while (ReadFile(handle, buffer, sizeof(buffer), &read_count, nullptr) && read_count > 0) {
      std::lock_guard<std::mutex> lock(output_mutex);
      append_output(output,
                    buffer,
                    read_count,
                    handler,
                    line_handler,
                    pending_line,
                    options,
                    result);
    }
  };
  std::thread stdout_thread(read_pipe,
                            stdout_read,
                            std::ref(result.stdout_text),
                            std::cref(options.stdout_handler),
                            std::cref(options.stdout_line_handler),
                            std::ref(stdout_pending_line));
  std::thread stderr_thread(read_pipe,
                            stderr_read,
                            std::ref(result.stderr_text),
                            std::cref(options.stderr_handler),
                            std::cref(options.stderr_line_handler),
                            std::ref(stderr_pending_line));

  const DWORD wait_ms = options.timeout_ms > 0 ? static_cast<DWORD>(options.timeout_ms) : INFINITE;
  if (WaitForSingleObject(process.hProcess, wait_ms) == WAIT_TIMEOUT) {
    result.timed_out = true;
    TerminateProcess(process.hProcess, 1);
    WaitForSingleObject(process.hProcess, INFINITE);
  }

  stdout_thread.join();
  stderr_thread.join();
  flush_pending_output_line(stdout_pending_line, options.stdout_line_handler);
  flush_pending_output_line(stderr_pending_line, options.stderr_line_handler);

  DWORD exit_code = 0;
  if (GetExitCodeProcess(process.hProcess, &exit_code)) {
    result.exited    = true;
    result.exit_code = static_cast<int>(exit_code);
  }

  CloseHandle(stdout_read);
  CloseHandle(stderr_read);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return result;
}
#endif

}  // namespace

ProcessResult run_command(const std::string& executable,
                          const std::vector<std::string>& args,
                          const ProcessOptions& options) {
  return run_spawned_command(executable, args, options);
}

ProcessResult run_shell(const std::string& command, const ProcessOptions& options) {
#if defined(_WIN32)
  return run_command("cmd.exe", {"/C", command}, options);
#else
  return run_command("/bin/sh", {"-c", command}, options);
#endif
}

ParseResult parse_json_output(const std::string& text) {
  return serialization::parse_json_output(text);
}

ParseResult parse_yaml_output(const std::string& text) {
  return serialization::parse_yaml_output(text);
}

ParseResult parse_json_file(const std::string& path) {
  return serialization::parse_json_file(path);
}

ParseResult parse_yaml_file(const std::string& path) {
  return serialization::parse_yaml_file(path);
}

std::string output_value_to_string(const OutputValue& value) {
  return serialization::output_value_to_string(value);
}

}  // namespace platform::process
