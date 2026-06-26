/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "process_service.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>

#include "logger.h"

#if APP_USE_LIBCJSON
#if __has_include(<cjson/cJSON.h>)
#include <cjson/cJSON.h>
#else
#include <cJSON.h>
#endif
#endif

#if APP_USE_LIBYAML
#include <yaml.h>
#endif

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

std::string trim(std::string value) {
  const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
  return value;
}

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

std::string read_file(const std::string& path, std::string& error_message) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error_message = "failed to open file: " + path;
    return {};
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

bool parse_number(const std::string& text, double& value) {
  if (text.empty()) {
    return false;
  }

  char* end = nullptr;
  errno     = 0;
  value     = std::strtod(text.c_str(), &end);
  return errno == 0 && end && *end == '\0' && std::isfinite(value);
}

OutputValue scalar_from_text(const std::string& value) {
  const auto text = trim(value);
  if (text == "null" || text == "Null" || text == "NULL" || text == "~") {
    return OutputValue::null();
  }
  if (text == "true" || text == "True" || text == "TRUE") {
    return OutputValue::boolean(true);
  }
  if (text == "false" || text == "False" || text == "FALSE") {
    return OutputValue::boolean(false);
  }

  double number = 0.0;
  if (parse_number(text, number)) {
    return OutputValue::number(number);
  }
  return OutputValue::string(value);
}

std::string escape_string(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const auto ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }
  return escaped;
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

#if APP_USE_LIBCJSON
OutputValue output_value_from_json(const cJSON* item) {
  if (!item || cJSON_IsNull(item)) {
    return OutputValue::null();
  }
  if (cJSON_IsBool(item)) {
    return OutputValue::boolean(cJSON_IsTrue(item));
  }
  if (cJSON_IsNumber(item)) {
    return OutputValue::number(item->valuedouble);
  }
  if (cJSON_IsString(item)) {
    return OutputValue::string(item->valuestring ? item->valuestring : "");
  }
  if (cJSON_IsArray(item)) {
    std::vector<OutputValue> values;
    const auto size = cJSON_GetArraySize(item);
    values.reserve(static_cast<std::size_t>(std::max(size, 0)));
    cJSON* child = nullptr;
    cJSON_ArrayForEach(child, item) { values.push_back(output_value_from_json(child)); }
    return OutputValue::array(std::move(values));
  }
  if (cJSON_IsObject(item)) {
    std::map<std::string, OutputValue> values;
    cJSON* child = nullptr;
    cJSON_ArrayForEach(child, item) {
      values.emplace(child->string ? child->string : "", output_value_from_json(child));
    }
    return OutputValue::object(std::move(values));
  }
  return OutputValue::null();
}
#endif

#if APP_USE_LIBYAML
struct YamlEventOwner {
  yaml_event_t event{};
  bool initialized{false};

  ~YamlEventOwner() {
    if (initialized) {
      yaml_event_delete(&event);
    }
  }
};

bool next_yaml_event(yaml_parser_t& parser, YamlEventOwner& owner, std::string& error_message) {
  if (!yaml_parser_parse(&parser, &owner.event)) {
    error_message = parser.problem ? parser.problem : "failed to parse YAML";
    return false;
  }
  owner.initialized = true;
  return true;
}

OutputValue parse_yaml_node(yaml_parser_t& parser, std::string& error_message);

OutputValue parse_yaml_mapping(yaml_parser_t& parser, std::string& error_message) {
  std::map<std::string, OutputValue> values;
  while (error_message.empty()) {
    YamlEventOwner key_event;
    if (!next_yaml_event(parser, key_event, error_message)) {
      return OutputValue::null();
    }
    if (key_event.event.type == YAML_MAPPING_END_EVENT) {
      return OutputValue::object(std::move(values));
    }
    if (key_event.event.type != YAML_SCALAR_EVENT) {
      error_message = "YAML mapping key must be a scalar";
      return OutputValue::null();
    }

    auto* raw_value       = key_event.event.data.scalar.value;
    const auto raw_length = key_event.event.data.scalar.length;
    std::string key(reinterpret_cast<const char*>(raw_value), raw_length);
    auto value = parse_yaml_node(parser, error_message);
    if (!error_message.empty()) {
      return OutputValue::null();
    }
    values.emplace(std::move(key), std::move(value));
  }
  return OutputValue::null();
}

OutputValue parse_yaml_sequence(yaml_parser_t& parser, std::string& error_message) {
  std::vector<OutputValue> values;
  while (error_message.empty()) {
    YamlEventOwner event;
    if (!next_yaml_event(parser, event, error_message)) {
      return OutputValue::null();
    }
    if (event.event.type == YAML_SEQUENCE_END_EVENT) {
      return OutputValue::array(std::move(values));
    }

    switch (event.event.type) {
      case YAML_SCALAR_EVENT: {
        auto* raw_value       = event.event.data.scalar.value;
        const auto raw_length = event.event.data.scalar.length;
        values.push_back(
            scalar_from_text(std::string(reinterpret_cast<const char*>(raw_value), raw_length)));
        break;
      }
      case YAML_SEQUENCE_START_EVENT:
        values.push_back(parse_yaml_sequence(parser, error_message));
        break;
      case YAML_MAPPING_START_EVENT:
        values.push_back(parse_yaml_mapping(parser, error_message));
        break;
      default:
        error_message = "unsupported YAML sequence node";
        return OutputValue::null();
    }
  }
  return OutputValue::null();
}

OutputValue parse_yaml_node(yaml_parser_t& parser, std::string& error_message) {
  YamlEventOwner event;
  if (!next_yaml_event(parser, event, error_message)) {
    return OutputValue::null();
  }

  switch (event.event.type) {
    case YAML_SCALAR_EVENT: {
      auto* raw_value       = event.event.data.scalar.value;
      const auto raw_length = event.event.data.scalar.length;
      return scalar_from_text(std::string(reinterpret_cast<const char*>(raw_value), raw_length));
    }
    case YAML_SEQUENCE_START_EVENT:
      return parse_yaml_sequence(parser, error_message);
    case YAML_MAPPING_START_EVENT:
      return parse_yaml_mapping(parser, error_message);
    case YAML_DOCUMENT_END_EVENT:
      return OutputValue::null();
    default:
      error_message = "unsupported YAML node";
      return OutputValue::null();
  }
}
#endif

}  // namespace

OutputValue OutputValue::null() { return {}; }

OutputValue OutputValue::boolean(bool value) {
  OutputValue result;
  result.type       = Type::Boolean;
  result.bool_value = value;
  return result;
}

OutputValue OutputValue::number(double value) {
  OutputValue result;
  result.type         = Type::Number;
  result.number_value = value;
  return result;
}

OutputValue OutputValue::string(std::string value) {
  OutputValue result;
  result.type         = Type::String;
  result.string_value = std::move(value);
  return result;
}

OutputValue OutputValue::array(std::vector<OutputValue> value) {
  OutputValue result;
  result.type         = Type::Array;
  result.array_values = std::move(value);
  return result;
}

OutputValue OutputValue::object(std::map<std::string, OutputValue> value) {
  OutputValue result;
  result.type          = Type::Object;
  result.object_values = std::move(value);
  return result;
}

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
  ParseResult result;
#if APP_USE_LIBCJSON
  cJSON* root = cJSON_ParseWithLength(text.data(), text.size());
  if (!root) {
    const char* error = cJSON_GetErrorPtr();
    result.error_message =
        error ? std::string("JSON parse error near: ") + error : "failed to parse JSON";
    return result;
  }

  result.value = output_value_from_json(root);
  cJSON_Delete(root);
  return result;
#else
  (void)text;
  result.error_message = "cJSON library is not available";
  return result;
#endif
}

ParseResult parse_yaml_output(const std::string& text) {
  ParseResult result;
#if APP_USE_LIBYAML
  yaml_parser_t parser{};
  if (!yaml_parser_initialize(&parser)) {
    result.error_message = "failed to initialize YAML parser";
    return result;
  }

  yaml_parser_set_input_string(&parser,
                               reinterpret_cast<const unsigned char*>(text.data()),
                               text.size());

  YamlEventOwner stream_start;
  if (!next_yaml_event(parser, stream_start, result.error_message) ||
      stream_start.event.type != YAML_STREAM_START_EVENT) {
    yaml_parser_delete(&parser);
    if (result.error_message.empty()) {
      result.error_message = "invalid YAML stream";
    }
    return result;
  }

  YamlEventOwner document_start;
  if (!next_yaml_event(parser, document_start, result.error_message) ||
      document_start.event.type != YAML_DOCUMENT_START_EVENT) {
    yaml_parser_delete(&parser);
    if (result.error_message.empty()) {
      result.error_message = "invalid YAML document";
    }
    return result;
  }

  result.value = parse_yaml_node(parser, result.error_message);
  yaml_parser_delete(&parser);
  return result;
#else
  (void)text;
  result.error_message = "libyaml is not available";
  return result;
#endif
}

ParseResult parse_json_file(const std::string& path) {
  std::string error_message;
  auto content = read_file(path, error_message);
  if (!error_message.empty()) {
    ParseResult result;
    result.error_message = std::move(error_message);
    return result;
  }
  return parse_json_output(content);
}

ParseResult parse_yaml_file(const std::string& path) {
  std::string error_message;
  auto content = read_file(path, error_message);
  if (!error_message.empty()) {
    ParseResult result;
    result.error_message = std::move(error_message);
    return result;
  }
  return parse_yaml_output(content);
}

std::string output_value_to_string(const OutputValue& value) {
  switch (value.type) {
    case OutputValue::Type::Null:
      return "null";
    case OutputValue::Type::Boolean:
      return value.bool_value ? "true" : "false";
    case OutputValue::Type::Number: {
      std::ostringstream out;
      out << value.number_value;
      return out.str();
    }
    case OutputValue::Type::String:
      return "\"" + escape_string(value.string_value) + "\"";
    case OutputValue::Type::Array: {
      std::string out = "[";
      bool first      = true;
      for (const auto& item : value.array_values) {
        if (!first) {
          out += ", ";
        }
        first = false;
        out += output_value_to_string(item);
      }
      out += "]";
      return out;
    }
    case OutputValue::Type::Object: {
      std::string out = "{";
      bool first      = true;
      for (const auto& item : value.object_values) {
        if (!first) {
          out += ", ";
        }
        first = false;
        out += "\"" + escape_string(item.first) + "\": " + output_value_to_string(item.second);
      }
      out += "}";
      return out;
    }
  }
  return "null";
}

}  // namespace platform::process
