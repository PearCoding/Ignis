#pragma once

#include "device/ICompilerDevice.h"

namespace IG {
class Compiler : public ICompilerDevice {
public:
    bool compile(const Settings& settings, const std::string& script) const override;
    void* compileAndGet(const Settings& settings, const std::string& script, const std::string& function) const override;
};
} // namespace IG