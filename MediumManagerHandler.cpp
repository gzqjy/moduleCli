#include "MediumManagerHandler.h"
#include "logger.h"

bool MediumManagerHandler::recover_hardware() const {
    SPDLOG_INFO("MediumManager: Executing specific hardware recovery on stop.");
    // TODO: 调用相关的 Windows API 或脚本来重置 USB/外设状态
    return true;
}

std::vector<std::string> MediumManagerHandler::get_backup_files() const {
    // TODO: 请在这里填入介质管控专属的备份文件列表
    // 比如：return {"medium_config.json", "diskStat.json"};
    return {"diskStat.json", "globalconfig.db", "hostEnv.json"}; // 先填入旧的默认配置防崩溃
}
