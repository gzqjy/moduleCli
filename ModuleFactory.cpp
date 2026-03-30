#include "ModuleFactory.h"
#include "MediumManagerHandler.h"
#include "PluginManagerHandler.h"

std::unique_ptr<ModuleHandler> create_module_handler(const std::string& module_name) {
    if (module_name == "MediumManager") {
        return std::unique_ptr<ModuleHandler>(new MediumManagerHandler(module_name));
    }
    
    if (module_name == "PluginManager") {
        return std::unique_ptr<ModuleHandler>(new PluginManagerHandler(module_name));
    }

    // Default handler for all other modules
    return std::unique_ptr<ModuleHandler>(new ModuleHandler(module_name));
}
