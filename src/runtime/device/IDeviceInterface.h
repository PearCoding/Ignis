#pragma once

#include "ICompilerDevice.h"
#include "IRenderDevice.h"
#include "config/Build.h"

namespace IG {

class IG_LIB IDeviceInterface {
public:
    virtual ~IDeviceInterface() = default;

    virtual void makeCurrent() const = 0;

    [[nodiscard]] virtual Build::Version getVersion() const                                                     = 0;
    [[nodiscard]] virtual TargetArchitecture getArchitecture() const                                            = 0;
    [[nodiscard]] virtual IRenderDevice* createRenderDevice(const IRenderDevice::SetupSettings& settings) const = 0;
    [[nodiscard]] virtual ICompilerDevice* createCompilerDevice() const                                         = 0;
};

} // namespace IG