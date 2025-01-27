#pragma once

#include "IG_Config.h"

namespace IG {
class Runtime;
class Device;

class IG_LIB OIDN {
public:
    OIDN(Runtime* runtime);
    ~OIDN();

    void run(Device* device);

    [[nodiscard]] static bool isAvailable();
    [[nodiscard]] static bool hasGPU();

private:
    std::unique_ptr<class OIDNContext> mInternal;
};
} // namespace IG