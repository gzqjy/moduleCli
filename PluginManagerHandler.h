#pragma once
#include "ModuleHandler.h"

class PluginManagerHandler : public ModuleHandler {
public:
    using ModuleHandler::ModuleHandler;

    std::vector<std::string> get_backup_files() const override;
};
