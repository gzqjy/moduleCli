#include "PluginLoaderHandler.h"

#include <string>
#include <vector>

std::vector<std::string> PluginLoaderHandler::get_backup_files() const {
    return {"zkjs_plugins.db", "pm_log.db"};
}
