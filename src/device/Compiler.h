#pragma once

#include "device/ICompilerDevice.h"

namespace IG {
class Compiler : public ICompilerDevice {
public:
    void* compile(const Settings& settings, const std::string& script, const std::string& function) const override;
};
} // namespace IG