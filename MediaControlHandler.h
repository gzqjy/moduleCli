#pragma once
#include "ModuleHandler.h"

class MediaControlHandler : public ModuleHandler {
public:
    using ModuleHandler::ModuleHandler;

    // We can override specific operations for Media Control here.
    // bool stop() override;
    // bool recover_hardware() const override;
};
