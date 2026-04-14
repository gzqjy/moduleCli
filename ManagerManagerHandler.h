#pragma once
#include "ModuleHandler.h"

class ManagerManagerHandler : public ModuleHandler {
public:
    using ModuleHandler::ModuleHandler;

protected:
    std::vector<std::string> get_backup_files() const override;
    bool recover_hardware() const override;
};
