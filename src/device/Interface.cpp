#include "Compiler.h"
#include "Device.h"
#include "config/Build.h"
#include "device/IDeviceInterface.h"
#include "device/Target.h"

namespace IG {
#if defined(IG_BUILD_DEVICE_CPU)
#if defined(IG_CPU_ARM)
#define CLASS_NAME DeviceInterface_ARM
#elif defined(IG_CPU_X86)
#define CLASS_NAME DeviceInterface_X86
#else
#error Unknown cpu architecture
#endif
#elif defined(IG_BUILD_DEVICE_CUDA)
#define CLASS_NAME DeviceInterface_CUDA
#elif defined(IG_BUILD_DEVICE_AMD_HSA)
#define CLASS_NAME DeviceInterface_AMD_HSA
#elif defined(IG_BUILD_DEVICE_AMD_PAL)
#define CLASS_NAME DeviceInterface_AMD_PAL
#elif defined(IG_BUILD_DEVICE_INTEL)
#define CLASS_NAME DeviceInterface_INTEL
#else
#error Unknown device architecture
#endif

class CLASS_NAME : public IDeviceInterface {
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
    static IG::CLASS_NAME interface;
    return &interface;
}
}