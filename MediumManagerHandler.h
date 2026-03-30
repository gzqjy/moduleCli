#pragma once
#include "ModuleHandler.h"

class MediumManagerHandler : public ModuleHandler {
public:
    using ModuleHandler::ModuleHandler;

    // We can override specific operations for MediumManager here.
    // bool stop() override;
    // bool recover_hardware() const override;
};
