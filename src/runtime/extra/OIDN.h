#pragma once

#include "IG_Config.h"

namespace IG {
class Runtime;
class IRenderDevice;

class IG_LIB OIDN {
public:
    OIDN(Runtime* runtime);
    ~OIDN();

    void run(IRenderDevice* device);

    [[nodiscard]] static bool isAvailable();

private:
    std::unique_ptr<class OIDNContext> mInternal;
};
} // namespace IG