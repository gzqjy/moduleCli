#include "ModuleHandler.h"

#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/process/v1.hpp>
#include <cctype>
#include <chrono>
#include <fstream>
#include <thread>

#include "logger.h"
#include "nlohmann_json.hpp"
#include "picosha2.h"
#include "utils.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <tlhelp32.h>
#include <windows.h>
#else
#include <dirent.h>
#include <signal.h>
#include <unistd.h>

#include <cstring>

#endif

namespace bp = boost::process;

// 掩值 (Linux 平台 SHA256 签名使用)
static const std::string kSignMask = "a3f7c2e1-9b84-4d6f-b5e0-1a2c3d4e5f60";

std::vector<int> ModuleHandler::find_module_pids() const {
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

bool ModuleHandler::send_signal(int pid, const std::string& signal) const {
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

bool ModuleHandler::is_process_alive(int pid) const {
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

bool ModuleHandler::start(const std::vector<std::string>& extra_args) {
    const std::string executable = resolve_module_binary(module_name);
    std::error_code ec;
    // try {
    // 1️⃣ 继承当前进程环境
    //         bp::environment env = boost::this_process::environment();
    //
    //         // 2️⃣ 构造 PATH 追加内容
    // #ifdef _WIN32
    //         const std::string sep = ";";
    //         const std::string custom_path = "C:\\myapp\\bin";
    // #else
    //         const std::string sep = ":";
    //         const std::string custom_path = "/opt/myapp/bin";
    // #endif
    //
    //         // 3️⃣ 处理 PATH（不存在时初始化）
    //         if (env.find("PATH") != env.end()) {
    //             std::string old_path = env["PATH"].to_string();
    //             env["PATH"] = old_path + sep + custom_path;
    //         } else {
    //             env["PATH"] = custom_path;
    //         }
    //
    //         // 4️⃣ 启动子进程
    //         bp::child proc(
    //             executable,
    //             bp::args(extra_args),
    //             env,  // 👈 注入环境变量
    //             bp::std_out > bp::null,
    //             bp::std_err > bp::null,
    //             ec
    //         );
    //
    //         if (ec) {
    //             SPDLOG_ERROR("Failed to start module '{}': {}", module_name, ec.message());
    //             return false;
    //         }
    //
    //         const int pid = static_cast<int>(proc.id());
    //
    //         // 5️⃣ 脱离子进程
    //         proc.detach();
    //
    //         SPDLOG_INFO("Started module '{}' with pid {}", module_name, pid);
    //
    //         return true;
    //
    //     } catch (const std::exception& e) {
    //         SPDLOG_ERROR("Exception while starting module '{}': {}", module_name, e.what());
    //         return false;
    //     }

    bp::child proc(executable, bp::args(extra_args), bp::std_out > bp::null, bp::std_err > bp::null, ec);
    if (ec) {
        SPDLOG_ERROR("Failed to start module '{}': {}", module_name, ec.message());
        return false;
    }

    const int pid = static_cast<int>(proc.id());
    proc.detach();
    SPDLOG_INFO("Started module '{}' with pid {}", module_name, pid);
    return true;
}

bool ModuleHandler::stop() {
    if (!recover_hardware()) {
        SPDLOG_ERROR("Hardware recovery failed for '{}', but continuing to stop.", module_name);
    }

    std::vector<int> pids = find_module_pids();
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

std::vector<std::string> ModuleHandler::get_backup_files() const {
    return {};
}

bool ModuleHandler::backup_files() const {
    std::vector<std::string> files = get_backup_files();
    if (files.empty()) return true;

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

    for (const auto& file : files) {
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

bool ModuleHandler::restore_files() const {
    std::vector<std::string> files = get_backup_files();
    if (files.empty()) return true;

    namespace fs = boost::filesystem;
    fs::path backup_dir = fs::path(get_executable_dir()) / "backup" / basename_of(module_name);

    for (const auto& file : files) {
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

bool ModuleHandler::recover_hardware() const {
    SPDLOG_INFO("Executing default hardware recovery operations for module '{}'", module_name);
    // TODO: Implement specific hardware device recovery logic here.
    return true;
}

bool ModuleHandler::preinst() {
    return backup_files();
}

bool ModuleHandler::postinst() {
    return restore_files();
}

bool ModuleHandler::preun() {
    return true;
}

bool ModuleHandler::postun() {
    return true;
}

bool ModuleHandler::update_sign() {
    namespace fs = boost::filesystem;
    const std::string exe_dir = get_executable_dir();
    fs::path mainfest_path = fs::path(exe_dir) / "mainfest.json";

    if (!fs::exists(mainfest_path)) {
        SPDLOG_ERROR("mainfest.json not found: {}", mainfest_path.string());
        return false;
    }

    // 读取文件
    std::ifstream ifs(mainfest_path.string());
    if (!ifs.is_open()) {
        SPDLOG_ERROR("Failed to open mainfest.json: {}", mainfest_path.string());
        return false;
    }

    nlohmann::json mainfest;
    try {
        ifs >> mainfest;
    } catch (const nlohmann::json::parse_error& e) {
        SPDLOG_ERROR("Failed to parse mainfest.json: {}", e.what());
        return false;
    }
    ifs.close();

    if (!mainfest.is_array()) {
        SPDLOG_ERROR("mainfest.json root is not an array");
        return false;
    }

    bool updated = false;
    for (auto& item : mainfest) {
        if (!item.is_object()) continue;
        if (!item.contains("name") || !item.contains("entry") || !item.contains("sign")) continue;
        if (item["name"].get<std::string>() != module_name) continue;

        const std::string entry = item["entry"].get<std::string>();

        // 确定二进制文件名（尝试 .exe / .dll / 无扩展名 / .so）
        std::string binary_name;
#ifdef _WIN32
        if (fs::exists(fs::path(exe_dir) / (entry + ".exe"))) {
            binary_name = ".\\" + entry + ".exe";
        } else if (fs::exists(fs::path(exe_dir) / (entry + ".dll"))) {
            binary_name = ".\\" + entry + ".dll";
        } else {
            SPDLOG_ERROR("Binary not found for entry: {}", entry);
            continue;
        }
#else
        if (fs::exists(fs::path(exe_dir) / entry)) {
            binary_name = "./" + entry;
        } else if (fs::exists(fs::path(exe_dir) / (entry + ".so"))) {
            binary_name = "./" + entry + ".so";
        } else {
            SPDLOG_ERROR("Binary not found for entry: {}", entry);
            continue;
        }
#endif

        // 计算签名
        std::string sign_value;

#ifdef _WIN32
        // Windows: 调用 sign_file.bat
        {
            bp::ipstream pipe_stream;
            std::error_code ec;
            std::string sign_script = ".\\sign_file.bat";
            bp::child proc(bp::search_path("cmd"), "/c", "call", sign_script, binary_name, bp::std_out > pipe_stream,
                           bp::std_err > bp::null, bp::start_dir(exe_dir), ec);

            if (ec) {
                SPDLOG_ERROR("Failed to run sign script for {}: {}", binary_name, ec.message());
                continue;
            }

            std::string line;
            while (std::getline(pipe_stream, line)) {
                if (!line.empty()) {
                    sign_value = line;
                }
            }
            proc.wait();

            // 去除尾部空白
            while (!sign_value.empty() &&
                   (sign_value.back() == '\r' || sign_value.back() == '\n' || sign_value.back() == ' ')) {
                sign_value.pop_back();
            }

            if (proc.exit_code() != 0) {
                SPDLOG_ERROR("sign script failed for {} with exit code {}", binary_name, proc.exit_code());
                continue;
            }
        }
#else
        // Linux: SHA256(文件内容 + 掩值) 采用分块读取优化大文件内存占用
        {
            std::ifstream bin_file((fs::path(exe_dir) / binary_name).string(), std::ios::binary);
            if (!bin_file.is_open()) {
                SPDLOG_ERROR("Failed to open binary for signing: {}", binary_name);
                continue;
            }

            picosha2::hash256_one_by_one hasher;
            constexpr size_t buffer_size = 1024 * 1024; // 1MB chunk size
            std::vector<char> buffer(buffer_size);

            while (bin_file) {
                bin_file.read(buffer.data(), buffer.size());
                std::streamsize read_count = bin_file.gcount();
                if (read_count > 0) {
                    hasher.process(buffer.begin(), buffer.begin() + read_count);
                }
            }
            bin_file.close();

            hasher.process(kSignMask.begin(), kSignMask.end());
            hasher.finish();
            sign_value = picosha2::get_hash_hex_string(hasher);
        }
#endif
        item["sign"] = sign_value;
        updated = true;
        SPDLOG_INFO("Updated sign for entry '{}': {}", entry, sign_value);
    }

    if (!updated) {
        SPDLOG_ERROR("No matching entries found for module '{}'", module_name);
        return false;
    }

    // 写回 mainfest.json（缩进2空格）
    std::ofstream ofs(mainfest_path.string());
    if (!ofs.is_open()) {
        SPDLOG_ERROR("Failed to write mainfest.json");
        return false;
    }
    ofs << mainfest.dump(2) << std::endl;
    ofs.close();

    SPDLOG_INFO("mainfest.json updated successfully for module '{}'", module_name);
    return true;
}
