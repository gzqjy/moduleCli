#include "ModuleFactory.h"
#include "utils.h"
#include "logger.h"

#include <iostream>
#include <vector>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

void print_usage(const char* program_name) {
    std::cout << "Usage: \n"
              << "  " << program_name << " <command> <magicnum> <module_name> [extra_args...]\n"
              << "  " << program_name << " generate_token\n"
              << "Commands: start | stop | sign | preinst | postinst | preun | postun" << std::endl;
}

bool handle_commands(const std::string& command, const std::string& module_name, const std::vector<std::string>& extra_args) {
    if (module_name.empty()) {
        SPDLOG_ERROR("Missing module_name argument.");
        return false;
    }
    auto handler = create_module_handler(module_name);

    if (command == "start") return handler->start(extra_args);
    if (command == "stop") return handler->stop();
    if (command == "sign") return handler->update_sign();
    if (command == "preinst") return handler->preinst();
    if (command == "postinst") return handler->postinst();
    if (command == "preun") return handler->preun();
    if (command == "postun") return handler->postun();

    SPDLOG_ERROR("Unsupported command: {}", command);
    return false;
}

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

    // 1. 基础校验 (必须包含 command magicnum module_name)
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    // 2. 直接提取位置参数
    std::string command = argv[1];
    std::string magicnum = argv[2];

    // 3. 提取 module_name 及剩余的 arg1, arg2...
    std::string module_name = argv[3];
    std::vector<std::string> extra_args;
    extra_args.reserve(argc - 4);
    for (int i = 4; i < argc; ++i) {
        extra_args.push_back(argv[i]);
    }

    // 4. Token 校验
    if (!verify_token(magicnum)) {
        SPDLOG_ERROR("Token 校验失败！");
        return 2;
    }

    // 5. 执行命令逻辑
    if (!handle_commands(command, module_name, extra_args)) {
        return 3;
    }

    SPDLOG_INFO("Command executed successfully.");
    return 0;
}
