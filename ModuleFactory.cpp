#include "ModuleFactory.h"

#include "ManagerManagerHandler.h"
#include "PluginLoaderHandler.h"

#include <algorithm>
#include <cctype>

std::unique_ptr<ModuleHandler> create_module_handler(const std::string& module_name) {
    std::string lower_name = module_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (lower_name.find("medium") != std::string::npos || lower_name.find("device") != std::string::npos) {
        return std::unique_ptr<ModuleHandler>(new ManagerManagerHandler(module_name));
    }

    if (lower_name.find("plugin") != std::string::npos || lower_name.find("user") != std::string::npos) {
        return std::unique_ptr<ModuleHandler>(new PluginLoaderHandler(module_name));
    }

    // Default handler for all other modules
    return std::unique_ptr<ModuleHandler>(new ModuleHandler(module_name));
}
