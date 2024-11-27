#include "Compiler.h"
#include "Device.h"
#include "Logger.h"
#include "RuntimeInfo.h"
#include "config/Build.h"
#include "device/IDeviceInterface.h"
#include "device/Target.h"

#include <anydsl_jit.h>

extern "C" IG_EXPORT const IG::IDeviceInterface* ig_get_interface();

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

    void makeCurrent() const
    {
        const auto module_path = RuntimeInfo::modulePath((void*)ig_get_interface);
        if (!module_path.empty()) {
            IG_LOG(L_DEBUG) << "Loading symbolic module " << module_path << std::endl;
            anydsl_link(module_path.generic_string().c_str());
        }

        const auto runtime_path = RuntimeInfo::modulePath();
        if (!runtime_path.empty() && module_path != runtime_path) {
            IG_LOG(L_DEBUG) << "Loading symbolic module " << runtime_path << std::endl;
            anydsl_link(runtime_path.generic_string().c_str());
        }

        const auto exe_path = RuntimeInfo::executablePath();
        if (!exe_path.empty() && module_path != exe_path) {
            IG_LOG(L_DEBUG) << "Loading symbolic module " << exe_path << std::endl;
            anydsl_link(exe_path.generic_string().c_str());
        }

        const auto cache_dir = RuntimeInfo::cacheDirectory();
        anydsl_set_cache_directory(cache_dir.generic_string().c_str());
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