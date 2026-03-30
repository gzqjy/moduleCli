#pragma once
#include "ModuleHandler.h"

class MediumManagerHandler : public ModuleHandler {
public:
    using ModuleHandler::ModuleHandler;

    bool recover_hardware() const override;
    std::vector<std::string> get_backup_files() const override;
};
