#pragma once

#include "IG_Config.h"

namespace IG {

class ICompilerDevice {
public:
    struct Settings {
        int OptimizationLevel = 3;
        bool Verbose          = false;
    };

    virtual void* compile(const std::string& script, const std::string& function) = 0;
};

} // namespace IG