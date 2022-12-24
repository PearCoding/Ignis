#include "Target.h"
#include "StringUtils.h"

#include "anydsl_runtime.h"

#if defined(IG_OS_WINDOWS)
#include <intrin.h>
#endif

namespace IG {
Target::Target()
    : mInitialized(false)
    , mGPU(false)
    , mGPUArchitecture(GPUArchitecture::Unknown)
    , mCPUArchitecture(CPUArchitecture::Unknown)
    , mDevice(0)
    , mThreadCount(0)
    , mVectorWidth(1)
{
}

std::string Target::toString() const
{
    std::stringstream stream;
    if (mGPU) {
        stream << "GPU[";
        switch (mGPUArchitecture) {
        case GPUArchitecture::AMD:
            stream << "AMD";
            break;
        case GPUArchitecture::Intel:
            stream << "Intel";
            break;
        case GPUArchitecture::Nvidia:
            stream << "Nvidia";
            break;
        case GPUArchitecture::OpenCL:
            stream << "OpenCL";
            break;
        default:
        case GPUArchitecture::Unknown:
            stream << "Unknown";
            break;
        }
        stream << ",D=" << mDevice << "]";
    } else {
        stream << "CPU[";
        switch (mCPUArchitecture) {
        case CPUArchitecture::ARM:
            stream << "ARM";
            break;
        case CPUArchitecture::X86:
            stream << "x86";
            break;
        default:
        case CPUArchitecture::Unknown:
            stream << "Unknown";
            break;
        }
        stream << ",T=" << mThreadCount << ",V=" << mVectorWidth << "]";
    }
    return stream.str();
}

CPUArchitecture Target::getCPUArchitectureFromString(const std::string& str)
{
    const std::string lstr = to_lowercase(str);
    if (lstr == "arm")
        return CPUArchitecture::ARM;
    else if (lstr == "x86")
        return CPUArchitecture::X86;
    else
        return CPUArchitecture::Unknown;
}

GPUArchitecture Target::getGPUArchitectureFromString(const std::string& str)
{
    const std::string lstr = to_lowercase(str);
    if (lstr == "amd")
        return GPUArchitecture::AMD;
    else if (lstr == "intel")
        return GPUArchitecture::Intel;
    else if (lstr == "nvidia")
        return GPUArchitecture::Nvidia;
    else if (lstr == "opencl")
        return GPUArchitecture::OpenCL;
    else
        return GPUArchitecture::Unknown;
}

std::vector<std::string> Target::getAvailableCPUArchitectureNames()
{
    return { "ARM", "x86" };
}

std::vector<std::string> Target::getAvailableGPUArchitectureNames()
{
    return { "AMD", "Intel", "Nvidia", "OpenCL" };
}

static inline CPUArchitecture getCPUArchitecture()
{
#if defined(IG_CPU_ARM)
    return CPUArchitecture::ARM;
#elif defined(IG_CPU_X86)
    return CPUArchitecture::X86;
#else
    return CPUArchitecture::Unknown;
#endif
}

Target Target::makeGeneric()
{
    return makeCPU(0, 1);
}

Target Target::makeSingle()
{
    return makeCPU(1, 1);
}

Target Target::makeCPU(size_t threads, size_t vectorWidth)
{
    return makeCPU(getCPUArchitecture(), threads, vectorWidth);
}

Target Target::makeCPU(CPUArchitecture arch, size_t threads, size_t vectorWidth)
{
    Target target;
    target.mCPUArchitecture = arch;
    target.mThreadCount     = threads;
    target.mVectorWidth     = vectorWidth;
    target.mInitialized     = true;
    return target;
}

Target Target::makeGPU(GPUArchitecture vendor, size_t device)
{
    Target target;
    target.mGPU             = true;
    target.mGPUArchitecture = vendor;
    target.mDevice          = device;
    target.mInitialized     = true;
    return target;
}

enum CPUID_Mode {
    CM_1_EDX = 0,
    CM_1_ECX,
    CM_7_0_EBX,
    CM_7_0_ECX,
    CM_7_0_EDX,
    CM_7_1_EAX,
    CM_80000001_EDX,
    CM_80000001_ECX,
    _CM_COUNT
};

struct SearchEntry {
    const char* Name;
    CPUID_Mode Mode;
    uint32_t Bit;
};

constexpr uint32_t bit(uint32_t i) { return 1 << i; }

template <typename V, typename... T>
constexpr auto array_of(T&&... t)
    -> std::array<V, sizeof...(T)>
{
    return { { std::forward<T>(t)... } };
}

static const auto Entries = array_of<SearchEntry>(
    SearchEntry{ "MMX", CM_1_EDX, bit(23) },
    SearchEntry{ "SSE", CM_1_EDX, bit(25) },
    SearchEntry{ "SSE2", CM_1_EDX, bit(26) },
    //-------
    SearchEntry{ "SSE3", CM_1_ECX, bit(0) },
    SearchEntry{ "SSSE3", CM_1_ECX, bit(9) },
    SearchEntry{ "FMA", CM_1_ECX, bit(12) },
    SearchEntry{ "SSE4_1", CM_1_ECX, bit(19) },
    SearchEntry{ "SSE4_2", CM_1_ECX, bit(20) },
    SearchEntry{ "POPCNT", CM_1_ECX, bit(23) },
    SearchEntry{ "AVX", CM_1_ECX, bit(28) },
    //-------
    SearchEntry{ "BMI1", CM_7_0_EBX, bit(3) },
    SearchEntry{ "HLE", CM_7_0_EBX, bit(4) },
    SearchEntry{ "AVX2", CM_7_0_EBX, bit(5) },
    SearchEntry{ "BMI2", CM_7_0_EBX, bit(8) },
    SearchEntry{ "RTM", CM_7_0_EBX, bit(11) },
    SearchEntry{ "AVX512F", CM_7_0_EBX, bit(16) },
    SearchEntry{ "AVX512DQ", CM_7_0_EBX, bit(17) },
    SearchEntry{ "AVX512IFMA", CM_7_0_EBX, bit(21) },
    SearchEntry{ "AVX512PF", CM_7_0_EBX, bit(26) },
    SearchEntry{ "AVX512ER", CM_7_0_EBX, bit(27) },
    SearchEntry{ "AVX512CD", CM_7_0_EBX, bit(28) },
    SearchEntry{ "AVX512BW", CM_7_0_EBX, bit(30) },
    SearchEntry{ "AVX512VL", CM_7_0_EBX, bit(31) },
    //-------
    SearchEntry{ "AVX512VBMI", CM_7_0_ECX, bit(1) },
    SearchEntry{ "AVX512VBMI2", CM_7_0_ECX, bit(6) },
    SearchEntry{ "AVX512VNNI", CM_7_0_ECX, bit(11) },
    SearchEntry{ "AVX512BITALG", CM_7_0_ECX, bit(12) },
    SearchEntry{ "AVX512VPOPCNTDQ", CM_7_0_ECX, bit(14) },
    //-------
    SearchEntry{ "AVX5124VNNIW", CM_7_0_EDX, bit(2) },
    SearchEntry{ "AVX5124FMAPS", CM_7_0_EDX, bit(3) },
    //-------
    SearchEntry{ "AVX512BF16", CM_7_1_EAX, bit(5) },
    //-------
    SearchEntry{ "RDTSCP", CM_80000001_EDX, bit(27) },
    //-------
    SearchEntry{ "ABM", CM_80000001_EDX, bit(5) },
    SearchEntry{ "FMA4", CM_80000001_EDX, bit(16) },
    SearchEntry{ "TBM", CM_80000001_EDX, bit(21) });

using ResultVector = std::array<bool, Entries.size()>;

[[maybe_unused]] static bool getEntryFromResult(const char* name, const ResultVector& result)
{
    for (size_t i = 0; i < Entries.size(); ++i) {
        if (std::strcmp(Entries[i].Name, name) == 0)
            return result[i];
    }
    return false;
}

struct CPUIDOut {
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
};

static inline CPUIDOut i_cpuid(uint32_t eax)
{
#if defined(IG_OS_LINUX)
    CPUIDOut out;
    asm volatile("cpuid"
                 : "=a"(out.EAX),
                   "=b"(out.EBX),
                   "=c"(out.ECX),
                   "=d"(out.EDX)
                 : "0"(eax));
    return out;
#elif defined(IG_OS_APPLE)
    return CPUIDOut{ 0, 0, 0, 0 };
#elif defined(IG_OS_WINDOWS)
    int cpuinfo[4];
    __cpuid(cpuinfo, eax);
    return CPUIDOut{ (uint32_t)cpuinfo[0], (uint32_t)cpuinfo[1], (uint32_t)cpuinfo[2], (uint32_t)cpuinfo[3] };
#endif
}

static inline CPUIDOut i_cpuid(uint32_t eax, uint32_t ecx)
{
#if defined(IG_OS_LINUX)
    CPUIDOut out;
    asm volatile("cpuid"
                 : "=a"(out.EAX),
                   "=b"(out.EBX),
                   "=c"(out.ECX),
                   "=d"(out.EDX)
                 : "0"(eax), "2"(ecx));
    return out;
#elif defined(IG_OS_APPLE)
    return CPUIDOut{ 0, 0, 0, 0 };
#elif defined(IG_OS_WINDOWS)
    int cpuinfo[4];
    __cpuidex(cpuinfo, eax, ecx);
    return CPUIDOut{ (uint32_t)cpuinfo[0], (uint32_t)cpuinfo[1], (uint32_t)cpuinfo[2], (uint32_t)cpuinfo[3] };
#endif
}

static void checkByBits(CPUID_Mode mode, uint32_t bits, ResultVector& results)
{
    for (size_t i = 0; i < Entries.size(); ++i) {
        if (Entries[i].Mode == mode)
            results[i] = Entries[i].Bit & bits;
    }
}

static bool has80000001()
{
    uint32_t highestExtF = i_cpuid(0x80000000).EAX;
    return highestExtF >= 0x80000001;
}

using CheckFunction                                              = void (*)(ResultVector& vector);
static const std::array<CheckFunction, _CM_COUNT> CheckFunctions = {
    [](ResultVector& results) { checkByBits(CM_1_EDX, i_cpuid(1).EDX, results); },
    [](ResultVector& results) { checkByBits(CM_1_ECX, i_cpuid(1).ECX, results); },
    [](ResultVector& results) { checkByBits(CM_7_0_EBX, i_cpuid(7, 0).EBX, results); },
    [](ResultVector& results) { checkByBits(CM_7_0_ECX, i_cpuid(7, 0).ECX, results); },
    [](ResultVector& results) { checkByBits(CM_7_0_EDX, i_cpuid(7, 0).EDX, results); },
    [](ResultVector& results) { checkByBits(CM_7_1_EAX, i_cpuid(7, 1).EAX, results); },
    [](ResultVector& results) { if(has80000001()) checkByBits(CM_80000001_ECX, i_cpuid(0x80000001).ECX, results); },
    [](ResultVector& results) { if(has80000001()) checkByBits(CM_80000001_EDX, i_cpuid(0x80000001).EDX, results); }
};

static inline ResultVector detectFeatures()
{
    ResultVector features;
#ifdef _OS_APPLE
    std::fill(features.begin(), features.end(), false);
#else
    for (int i = 0; i < _CM_COUNT; ++i)
        CheckFunctions[i](features);
#endif
    return features;
}

Target Target::pickCPU()
{
    return makeCPU(0, 1);
    // FIXME: There is a vectorization bug. Do not pick it by default for now
    /*
        const auto features = detectFeatures();

        const bool hasSSE2 = getEntryFromResult("SSE2", features);
        const bool hasAVX2 = getEntryFromResult("AVX2", features);
        // bool hasAVX512 = getEntryFromResult("AVX512F", features);

    #if defined(IG_CPU_ARM)
        // TODO
        size_t vectorWidth = 4;
    #else
        size_t vectorWidth = 1;
        // if (hasAVX512)
        //    vectorWidth = 16;
        // else
        if (hasAVX2)
            vectorWidth = 8;
        else if (hasSSE2)
            vectorWidth = 4;
    #endif

        return makeCPU(0, vectorWidth);
    */
}

Target Target::pickGPU(size_t device)
{
#ifdef AnyDSL_runtime_HAS_CUDA_SUPPORT
    const bool hasNvidiaSupport = true;
#else
    const bool hasNvidiaSupport = false;
#endif

#ifdef AnyDSL_runtime_HAS_HSA_SUPPORT
    const bool hasAMDSupport = true;
#else
    const bool hasAMDSupport    = false;
#endif

#ifdef AnyDSL_runtime_HAS_OPENCL_SUPPORT
    const bool hasCLSupport = true;
#else
    const bool hasCLSupport     = false;
#endif

    // TODO: Runtime check?

    if (hasNvidiaSupport)
        return makeGPU(GPUArchitecture::Nvidia, device);
    else if (hasAMDSupport)
        return makeGPU(GPUArchitecture::AMD, device);
    else if (hasCLSupport)
        return makeGPU(GPUArchitecture::OpenCL, device);
    else
        return makeGPU(GPUArchitecture::Unknown, device);
}

Target Target::pickBest()
{
    Target t = pickGPU();
    if (t.isValid() && t.mGPUArchitecture != GPUArchitecture::Unknown)
        return t;

    return pickCPU();
}
} // namespace IG