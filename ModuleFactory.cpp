#include "ModuleFactory.h"
#include "MediaControlHandler.h"
#include "UserProfileHandler.h"

std::unique_ptr<ModuleHandler> create_module_handler(const std::string& module_name) {
    if (module_name.find("media") != std::string::npos || module_name.find("device") != std::string::npos) {
        return std::unique_ptr<ModuleHandler>(new MediaControlHandler(module_name));
    }
    
    if (module_name.find("persona") != std::string::npos || module_name.find("profile") != std::string::npos) {
        return std::unique_ptr<ModuleHandler>(new UserProfileHandler(module_name));
    }

    // Default handler for all other modules
    return std::unique_ptr<ModuleHandler>(new ModuleHandler(module_name));
}
