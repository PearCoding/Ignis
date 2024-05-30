#pragma once

#include "ICompilerDevice.h"
#include "IRenderDevice.h"
#include "config/Build.h"

namespace IG {

class IDeviceInterface {
public:
    virtual Build::Version getVersion() const                                               = 0;
    virtual TargetArchitecture getArchitecture() const                                      = 0;
    virtual IRenderDevice* createRenderDevice(const IRenderDevice::SetupSettings& settings) = 0;
    virtual ICompilerDevice* createCompilerDevice()                                         = 0;
};

} // namespace IG