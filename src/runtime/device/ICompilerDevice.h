#pragma once

#include "IG_Config.h"

namespace IG {
class IG_LIB ICompilerDevice {
public:
    virtual ~ICompilerDevice() = default;

    struct Settings {
        int OptimizationLevel = 3;
        bool Verbose          = false;
    };

    [[nodiscard]] virtual void* compile(const Settings& settings, const std::string& script, const std::string& function) const = 0;
};

} // namespace IG