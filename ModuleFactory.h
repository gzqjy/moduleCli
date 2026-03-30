#pragma once
#include <memory>
#include <string>
#include "ModuleHandler.h"

std::unique_ptr<ModuleHandler> create_module_handler(const std::string& module_name);
