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
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include <cctype>
#endif

#include "logger.h"
#include "picosha2.h"

namespace {
namespace bp = boost::process;
constexpr const char* kTokenSalt = "4c61a9e9-bd52-40c8-91d3-5d37776e687d";
constexpr long long kTokenWindowSize = 60 * 60;
const std::vector<const char*> kBackupFiles = {"diskStat.json", "globalconfig.db", "hostEnv.json"};

std::string sha256_hex(const std::string& input) {
    return picosha2::hash256_hex_string(input);
}

std::string basename_of(const std::string& path) {
    const std::size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string get_executable_dir() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string full_path(path);
    return full_path.substr(0, full_path.find_last_of("\\/"));
#else
    char path[4096];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path));
    if (count != -1) {
        std::string full_path(path, count);
        return full_path.substr(0, full_path.find_last_of("/"));
    }
    return ".";
#endif
}

std::string resolve_module_binary(const std::string& module_name) {
    if (module_name.find_first_of("/\\") != std::string::npos) {
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
    DIR* dp = opendir("/proc");
    if (!dp) return pids;

    const pid_t self_pid = getpid();
    struct dirent* dirp;
    while ((dirp = readdir(dp)) != nullptr) {
        if (!isdigit(dirp->d_name[0])) continue;

        int pid = std::atoi(dirp->d_name);
        if (pid == self_pid) continue;

        std::string comm_path = std::string("/proc/") + dirp->d_name + "/comm";
        std::ifstream comm_file(comm_path);
        if (comm_file.is_open()) {
            std::string comm_name;
            std::getline(comm_file, comm_name);
            if (comm_name == process_name) {
                pids.push_back(pid);
            }
        }
    }
    closedir(dp);
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
        std::string stat_path = std::string("/proc/") + std::to_string(pid) + "/stat";
        std::ifstream stat_file(stat_path);
        if (stat_file.is_open()) {
            std::string line;
            std::getline(stat_file, line);
            size_t rparen_pos = line.find_last_of(')');
            if (rparen_pos != std::string::npos && rparen_pos + 2 < line.size()) {
                char state = line[rparen_pos + 2];
                if (state == 'Z') {
                    return false;
                }
            }
        }
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

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (is_process_alive(pid)) {
                SPDLOG_ERROR("Process pid {} is still alive after SIGKILL", pid);
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
    const auto now_sec =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const long long current_window = now_sec / kTokenWindowSize;
    return sha256_hex(std::to_string(current_window) + ":" + kTokenSalt);
}

bool verify_token(const std::string& user_token) {
    const auto now_sec =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const long long current_window = now_sec / kTokenWindowSize;

    const auto check = [&](long long win) { return user_token == sha256_hex(std::to_string(win) + ":" + kTokenSalt); };

    return check(current_window) || check(current_window - 1);
}

bool backup_files(const std::string& module_name) {
    namespace fs = boost::filesystem;
    fs::path backup_dir = fs::path(get_executable_dir()) / "backup" / basename_of(module_name);

    boost::system::error_code ec;
    if (!fs::exists(backup_dir)) {
        fs::create_directories(backup_dir, ec);
        if (ec) {
            SPDLOG_ERROR("Failed to create backup dir {}: {}", backup_dir.string(), ec.message());
            return false;
        }
    }

    for (const auto& file : kBackupFiles) {
        fs::path src = fs::path(get_executable_dir()) / file;
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
    fs::path backup_dir = fs::path(get_executable_dir()) / "backup" / basename_of(module_name);

    for (const auto& file : kBackupFiles) {
        fs::path src = backup_dir / file;
        fs::path dst = fs::path(get_executable_dir()) / file;
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
    std::string log_file = get_executable_dir() + "/moduleCli.log";
    MYLOG_RESET(log_file, 1024 * 1024, 3);
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
    extra_args.reserve(argc > 4 ? argc - 4 : 0);
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
