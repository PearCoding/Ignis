#include "Compiler.h"
#include "Device.h"
#include "config/Build.h"
#include "device/IDeviceInterface.h"
#include "device/Target.h"

namespace IG {
class DeviceInterface : public IDeviceInterface {
public:
    Build::Version getVersion() const
    {
        return Build::getVersion();
    }

    TargetArchitecture getArchitecture() const
    {
#if defined(IG_BUILD_DEVICE_CPU)
#if defined(IG_CPU_ARM)
        return CPUArchitecture::ARM;
#elif defined(IG_CPU_X86)
        return CPUArchitecture::X86;
#else
#error Unknown cpu architecture
#endif
#elif defined(IG_BUILD_DEVICE_CUDA)
        return GPUArchitecture::Nvidia;
#elif defined(IG_BUILD_DEVICE_AMD_HSA)
        return GPUArchitecture::AMD_HSA;
#elif defined(IG_BUILD_DEVICE_AMD_PAL)
        return GPUArchitecture::AMD_PAL;
#elif defined(IG_BUILD_DEVICE_INTEL)
        return GPUArchitecture::Intel;
#else
#error Unknown device architecture
#endif
    }

    IRenderDevice* createRenderDevice(const IRenderDevice::SetupSettings& settings) const
    {
        return new Device(settings);
    }

    ICompilerDevice* createCompilerDevice() const
    {
        return new Compiler();
    }
};
} // namespace IG

extern "C" {
IG_EXPORT const IG::IDeviceInterface* ig_get_interface()
{
    static IG::DeviceInterface interface;
    return &interface;
}
}