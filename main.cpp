#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/process/v1.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <tlhelp32.h>
#include <windows.h>

#include <boost/process/v1/detail/windows/environment.hpp>
#else
#include <signal.h>

#include <boost/process/v1/detail/posix/environment.hpp>
#include <cerrno>
#endif

#include "logger.h"
#include "picosha2.h"

namespace {
namespace bp = boost::process;

std::string sha256_hex(const std::string& input) {
    return picosha2::hash256_hex_string(input);
}

std::string basename_of(const std::string& path) {
    const std::size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string resolve_module_binary(const std::string& module_name) {
    if (module_name.find('/') != std::string::npos) {
        return module_name;
    }

    const auto resolved = bp::search_path(module_name);
    return resolved.empty() ? module_name : resolved.string();
}

std::vector<int> find_module_pids(const std::string& module_name) {
    std::vector<int> pids;
    const std::string process_name = basename_of(module_name);
#ifdef _WIN32
    const std::string image_name =
        process_name.find(".exe") == std::string::npos ? process_name + ".exe" : process_name;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnapshot, &pe32)) {
            const DWORD self_pid = GetCurrentProcessId();
            do {
                if (pe32.th32ProcessID > 0 && pe32.th32ProcessID != self_pid) {
                    char temp[MAX_PATH * 4];
                    int len = WideCharToMultiByte(CP_UTF8, 0, pe32.szExeFile, -1, temp, sizeof(temp), NULL, NULL);
                    if (len > 0) {
                        std::string exe_name(temp);
                        if (exe_name == image_name) {
                            pids.push_back(pe32.th32ProcessID);
                        }
                    }
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
#else
    bp::ipstream output;
    std::error_code ec;

    bp::child query(bp::search_path("pgrep"), "-x", process_name, bp::std_out > output, bp::std_err > bp::null, ec);
    if (ec) {
        SPDLOG_ERROR("Failed to execute pgrep for '{}': {}", process_name, ec.message());
        return pids;
    }

    query.wait();
    if (query.exit_code() != 0) {
        return pids;
    }

    std::string line;
    const int self_pid = static_cast<int>(boost::process::v1::detail::posix::get_id());
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

bool send_signal(int pid, const std::string& signal) {
#ifdef _WIN32
    if (signal == "-KILL") {
        HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (process != NULL) {
            BOOL result = TerminateProcess(process, 1);
            CloseHandle(process);
            if (result) return true;
        }
    }

    std::error_code ec;
    std::vector<std::string> args;
    args.push_back("/PID");
    args.push_back(std::to_string(pid));
    args.push_back("/T");
    if (signal == "-KILL") {
        args.push_back("/F");
    }
    const int rc =
        bp::system(bp::search_path("taskkill"), bp::args(args), bp::std_out > bp::null, bp::std_err > bp::null, ec);
    if (ec) return false;
    return rc == 0;
#else
    int sig = SIGTERM;
    if (signal == "-KILL") {
        sig = SIGKILL;
    }
    return kill(pid, sig) == 0;
#endif
}

bool is_process_alive(int pid) {
#ifdef _WIN32
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (process == NULL) {
        return false;
    }
    DWORD ret = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return ret == WAIT_TIMEOUT;
#else
    if (kill(pid, 0) == 0) {
        return true;
    }
    return errno == EPERM;
#endif
}

bool start_module(const std::string& module_name, const std::vector<std::string>& extra_args) {
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

bool stop_module(const std::string& module_name) {
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
        for (int retry = 0; retry < 10; ++retry) {
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

void print_usage(const char* program_name) {
    std::cout << "Usage: \n"
              << "  " << program_name << " <module_name> <command> <magicnum> [extra_args...]\n"
              << "  " << program_name << " generate_token\n"
              << "Commands: start | stop | preinst | postinst | preun | postun" << std::endl;
}

std::string generate_current_token() {
    const std::string salt = "4c61a9e9-bd52-40c8-91d3-5d37776e687d";
    const long long window_size = 60 * 60;
    const auto now_sec =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const long long current_window = now_sec / window_size;
    return sha256_hex(std::to_string(current_window) + ":" + salt);
}

bool verify_token(const std::string& user_token) {
    const std::string salt = "4c61a9e9-bd52-40c8-91d3-5d37776e687d";
    const long long window_size = 60 * 60;

    const auto now_sec =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const long long current_window = now_sec / window_size;

    const auto check = [&](long long win) { return user_token == sha256_hex(std::to_string(win) + ":" + salt); };

    return check(current_window) || check(current_window - 1);
}

bool backup_files(const std::string& module_name) {
    namespace fs = boost::filesystem;
    fs::path backup_dir = fs::temp_directory_path() / ("moduleCli_backup_" + basename_of(module_name));

    boost::system::error_code ec;
    if (!fs::exists(backup_dir)) {
        fs::create_directories(backup_dir, ec);
        if (ec) {
            SPDLOG_ERROR("Failed to create backup dir {}: {}", backup_dir.string(), ec.message());
            return false;
        }
    }

    const std::vector<std::string> files = {"diskStat.json", "globalconfig.db", "hostEnv.json"};
    for (const auto& file : files) {
        fs::path src = fs::current_path() / file;
        fs::path dst = backup_dir / file;
        if (fs::exists(src)) {
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                SPDLOG_ERROR("Failed to backup {}: {}", file, ec.message());
                return false;
            }
            SPDLOG_INFO("Backed up {} to {}", file, dst.string());
        }
    }
    return true;
}

bool restore_files(const std::string& module_name) {
    namespace fs = boost::filesystem;
    fs::path backup_dir = fs::temp_directory_path() / ("moduleCli_backup_" + basename_of(module_name));

    const std::vector<std::string> files = {"diskStat.json", "globalconfig.db", "hostEnv.json"};
    for (const auto& file : files) {
        fs::path src = backup_dir / file;
        fs::path dst = fs::current_path() / file;
        if (fs::exists(src)) {
            boost::system::error_code ec;
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                SPDLOG_ERROR("Failed to restore {}: {}", file, ec.message());
                return false;
            }
            SPDLOG_INFO("Restored {} from {}", file, src.string());
        }
    }
    return true;
}

bool recover_hardware(const std::string& module_name) {
    SPDLOG_INFO("Executing hardware recovery operations for module '{}'", module_name);
    // TODO: Implement specific hardware device recovery logic here.
    // For example, calling an external script or using Windows APIs to recover USB states.
    return true;
}

bool handle_commands(const std::string& module_name, const std::string& command,
                     const std::vector<std::string>& extra_args) {
    static const std::unordered_map<std::string, std::string> cmd_desc = {
        {"start", "Start module"},
        {"stop", "Stop module"},
        {"preinst", "Run pre-install actions (Backup configs)"},
        {"postinst", "Run post-install actions (Restore configs)"},
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
        if (!recover_hardware(module_name)) {
            SPDLOG_ERROR("Hardware recovery failed for '{}', but continuing to stop.", module_name);
        }
        return stop_module(module_name);
    }

    if (command == "preinst") {
        if (!backup_files(module_name)) {
            return false;
        }
    }

    if (command == "postinst") {
        if (!restore_files(module_name)) {
            return false;
        }
    }

    SPDLOG_INFO("{}: {} (Args count: {})", it->second, module_name, extra_args.size());
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // 将控制台设置为 UTF-8 模式
#endif
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
