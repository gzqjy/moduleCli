#pragma once
#include "ModuleHandler.h"

class PluginLoaderHandler : public ModuleHandler {
public:
    using ModuleHandler::ModuleHandler;

protected:
    std::vector<std::string> get_backup_files() const override;
};
