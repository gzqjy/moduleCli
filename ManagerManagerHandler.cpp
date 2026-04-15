#include "ManagerManagerHandler.h"

#include <string>
#include <vector>

#include "spdlog/spdlog.h"

std::vector<std::string> ManagerManagerHandler::get_backup_files() const {
    return {"diskStat.json", "globalconfig.db", "hostEnv.json"};
}

bool ManagerManagerHandler::recover_hardware() const {
    SPDLOG_DEBUG("ManagerManagerHandler specific hardware recovery executed.");
    return true;
}
