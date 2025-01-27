#pragma once

#include "Device.h"
#include "ICompilerDevice.h"
#include "IDeviceInterface.h"
#include "config/Build.h"

namespace IG {

class IG_LIB IPluginInterface {
public:
    virtual ~IPluginInterface() = default;

    virtual void makeCurrent() const = 0;

    [[nodiscard]] virtual Build::Version getVersion() const                                                                    = 0;
    [[nodiscard]] virtual TargetArchitecture getArchitecture() const                                                           = 0;
    [[nodiscard]] virtual std::shared_ptr<IDeviceInterface> createDeviceInterface(const Device::SetupSettings& settings) const = 0;
    [[nodiscard]] virtual std::shared_ptr<ICompilerDevice> createCompilerDevice() const                                        = 0;
};

} // namespace IG