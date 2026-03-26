#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <boost/process.hpp>
#include <boost/process/v1.hpp>

#ifdef _WIN32
#include <boost/process/v1/detail/windows/environment.hpp>
#else
#include <boost/process/v1/detail/posix/environment.hpp>
#endif

#include "logger.h"
#include "picosha2.h"

namespace {
namespace bp = boost::process;

std::string sha256_hex(const std::string &input) {
  return picosha2::hash256_hex_string(input);
}

std::string basename_of(const std::string &path) {
  const std::size_t pos = path.find_last_of("/\\");
  return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string resolve_module_binary(const std::string &module_name) {
  if (module_name.find('/') != std::string::npos) {
    return module_name;
  }

  const auto resolved = bp::search_path(module_name);
  return resolved.empty() ? module_name : resolved.string();
}

std::vector<int> find_module_pids(const std::string &module_name) {
  std::vector<int> pids;
  const std::string process_name = basename_of(module_name);
#ifdef _WIN32
  const std::string image_name = process_name.find(".exe") == std::string::npos
                                     ? process_name + ".exe"
                                     : process_name;
  std::string filter = "IMAGENAME eq " + image_name;
  bp::ipstream output;
  std::error_code ec;
  bp::child query(bp::search_path("tasklist"), "/FO", "CSV", "/NH", "/FI",
                  filter, bp::std_out > output, bp::std_err > bp::null, ec);
  if (ec) {
    SPDLOG_ERROR("Failed to execute tasklist for '{}': {}", image_name,
                 ec.message());
    return pids;
  }

  query.wait();
  if (query.exit_code() != 0) {
    return pids;
  }

  std::string line;
  const int self_pid =
      static_cast<int>(boost::process::v1::detail::windows::get_id());
  while (std::getline(output, line)) {
    if (line.empty() || line.front() != '"') {
      continue;
    }

    // CSV sample: "xxx.exe","1234","Console","1","12,344 K"
    const std::size_t first_comma = line.find(',');
    if (first_comma == std::string::npos || first_comma + 1 >= line.size()) {
      continue;
    }
    const std::size_t pid_start = line.find('"', first_comma + 1);
    if (pid_start == std::string::npos) {
      continue;
    }
    const std::size_t pid_end = line.find('"', pid_start + 1);
    if (pid_end == std::string::npos || pid_end <= pid_start + 1) {
      continue;
    }

    const int pid =
        std::atoi(line.substr(pid_start + 1, pid_end - pid_start - 1).c_str());
    if (pid > 0 && pid != self_pid) {
      pids.push_back(pid);
    }
  }
#else
  bp::ipstream output;
  std::error_code ec;

  bp::child query(bp::search_path("pgrep"), "-x", process_name,
                  bp::std_out > output, bp::std_err > bp::null, ec);
  if (ec) {
    SPDLOG_ERROR("Failed to execute pgrep for '{}': {}", process_name,
                 ec.message());
    return pids;
  }

  query.wait();
  if (query.exit_code() != 0) {
    return pids;
  }

  std::string line;
  const int self_pid =
      static_cast<int>(boost::process::v1::detail::posix::get_id());
  while (std::getline(output, line)) {
    if (line.empty()) {
      continue;
    }

    const int pid = std::atoi(line.c_str());
    if (pid > 0 && pid != self_pid) {
      pids.push_back(pid);
    }
  }
#endif

  return pids;
}

bool send_signal(int pid, const std::string &signal) {
  std::error_code ec;
#ifdef _WIN32
  std::vector<std::string> args;
  args.push_back("/PID");
  args.push_back(std::to_string(pid));
  args.push_back("/T");
  if (signal == "-KILL") {
    args.push_back("/F");
  }
  const int rc = bp::system(bp::search_path("taskkill"), bp::args(args),
                            bp::std_out > bp::null, bp::std_err > bp::null, ec);
  if (ec)
    return false;
  return rc == 0;
#else
  const int rc =
      bp::system(bp::search_path("kill"), signal, std::to_string(pid),
                 bp::std_out > bp::null, bp::std_err > bp::null, ec);
  if (ec)
    return false;
  return rc == 0;
#endif
}

bool is_process_alive(int pid) {
  std::error_code ec;
#ifdef _WIN32
  bp::ipstream output;
  std::string pid_filter = "PID eq " + std::to_string(pid);
  const int rc =
      bp::system(bp::search_path("tasklist"), "/FO", "CSV", "/NH", "/FI",
                 pid_filter, bp::std_out > output, bp::std_err > bp::null, ec);
  if (ec || rc != 0) {
    return false;
  }
  std::string line;
  while (std::getline(output, line)) {
    if (!line.empty() && line.front() == '"') {
      return true;
    }
  }
  return false;
#else
  const int rc = bp::system(bp::search_path("kill"), "-0", std::to_string(pid),
                            bp::std_out > bp::null, bp::std_err > bp::null, ec);
  if (ec)
    return false;
  return rc == 0;
#endif
}

bool start_module(const std::string &module_name,
                  const std::vector<std::string> &extra_args) {
  const std::string executable = resolve_module_binary(module_name);
  std::error_code ec;
  bp::child proc(executable, bp::args(extra_args), bp::std_out > bp::null,
                 bp::std_err > bp::null, ec);
  if (ec) {
    SPDLOG_ERROR("Failed to start module '{}': {}", module_name, ec.message());
    return false;
  }

  const int pid = static_cast<int>(proc.id());
  proc.detach();
  SPDLOG_INFO("Started module '{}' with pid {}", module_name, pid);
  return true;
}

bool stop_module(const std::string &module_name) {
  std::vector<int> pids = find_module_pids(module_name);
  if (pids.empty()) {
    SPDLOG_INFO("No running process matched module '{}'", module_name);
    return true;
  }

  bool all_stopped = true;
  for (std::size_t i = 0; i < pids.size(); ++i) {
    const int pid = pids[i];
    if (!send_signal(pid, "-TERM")) {
      SPDLOG_ERROR("Failed to SIGTERM pid {}", pid);
      all_stopped = false;
      continue;
    }

    bool terminated = false;
    for (int retry = 0; retry < 20; ++retry) {
      if (!is_process_alive(pid)) {
        terminated = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!terminated) {
      if (!send_signal(pid, "-KILL")) {
        SPDLOG_ERROR("Failed to SIGKILL pid {}", pid);
        all_stopped = false;
        continue;
      }
    }

    SPDLOG_INFO("Stopped module '{}' process pid {}", module_name, pid);
  }

  return all_stopped;
}

void print_usage(const char *program_name) {
  std::cout << "Usage: \n"
            << "  " << program_name << " <module_name> <command> <magicnum> [extra_args...]\n"
            << "  " << program_name << " generate_token\n"
            << "Commands: start | stop | preinst | postinst | preun | postun"
            << std::endl;
}

std::string generate_current_token() {
  const std::string salt = "4c61a9e9-bd52-40c8-91d3-5d37776e687d";
  const long long window_size = 60;
  const auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  const long long current_window = now_sec / window_size;
  return sha256_hex(std::to_string(current_window) + ":" + salt);
}

bool verify_token(const std::string &user_token) {
  const std::string salt = "4c61a9e9-bd52-40c8-91d3-5d37776e687d";
  const long long window_size = 60;

  const auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
  const long long current_window = now_sec / window_size;

  const auto check = [&](long long win) {
    return user_token == sha256_hex(std::to_string(win) + ":" + salt);
  };

  return check(current_window) || check(current_window - 1);
}

bool handle_commands(const std::string &module_name, const std::string &command,
                     const std::vector<std::string> &extra_args) {
  static const std::unordered_map<std::string, std::string> cmd_desc = {
      {"start", "Start module"},
      {"stop", "Stop module"},
      {"preinst", "Run pre-install actions"},
      {"postinst", "Run post-install actions"},
      {"preun", "Run pre-uninstall actions"},
      {"postun", "Run post-uninstall actions"}};

  const auto it = cmd_desc.find(command);
  if (it == cmd_desc.end()) {
    SPDLOG_ERROR("Unsupported command: {}", command);
    return false;
  }

  if (command == "start") {
    return start_module(module_name, extra_args);
  }

  if (command == "stop") {
    return stop_module(module_name);
  }

  SPDLOG_INFO("{}: {} (Args count: {})", it->second, module_name,
              extra_args.size());
  return true;
}

} // namespace

int main(int argc, char *argv[]) {
  MYLOG_RESET("moduleCli.log", 1024 * 1024, 3);
  MYLOG_LOG_LEVEL("debug");

  if (argc == 2 && std::string(argv[1]) == "generate_token") {
    std::cout << generate_current_token() << std::endl;
    return 0;
  }

  // 1. 基础校验 (必须包含 module_name command magicnum)
  if (argc < 4) {
    print_usage(argv[0]);
    return 1;
  }

  // 2. 直接提取位置参数
  std::string module_name = argv[1];
  std::string command = argv[2];
  std::string magicnum = argv[3];

  // 3. 提取剩余的 arg1, arg2...
  std::vector<std::string> extra_args;
  for (int i = 4; i < argc; ++i) {
    extra_args.push_back(argv[i]);
  }

  // 4. Token 校验
  if (!verify_token(magicnum)) {
    SPDLOG_ERROR("Token 校验失败！");
    return 2;
  }

  // 5. 执行命令逻辑
  if (!handle_commands(module_name, command, extra_args)) {
    return 3;
  }

  SPDLOG_INFO("Command executed successfully.");
  return 0;
}
