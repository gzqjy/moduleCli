#include "ModuleFactory.h"

#include "ManagerManagerHandler.h"
#include "PluginLoaderHandler.h"

std::unique_ptr<ModuleHandler> create_module_handler(const std::string& module_name) {
    if (module_name.find("medium") != std::string::npos || module_name.find("device") != std::string::npos) {
        return std::unique_ptr<ModuleHandler>(new ManagerManagerHandler(module_name));
    }

    if (module_name.find("plugin") != std::string::npos || module_name.find("user") != std::string::npos) {
        return std::unique_ptr<ModuleHandler>(new PluginLoaderHandler(module_name));
    }

    // Default handler for all other modules
    return std::unique_ptr<ModuleHandler>(new ModuleHandler(module_name));
}
