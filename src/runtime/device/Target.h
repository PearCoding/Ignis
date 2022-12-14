#pragma once

#include "IG_Config.h"

namespace IG {
enum class GPUVendor {
    AMD,
    Intel, // TODO: Only listed here, no support at the moment
    Nvidia,
    Unknown
};

enum class CPUArchitecture {
    ARM,
    X86,
    Unknown
};

class IG_LIB Target {
public:
    Target();

    [[nodiscard]] inline bool isValid() const { return mInitialized; }
    [[nodiscard]] inline bool requiresPadding() const { return isGPU(); }

    [[nodiscard]] inline bool isCPU() const { return !mGPU; }
    [[nodiscard]] inline bool isGPU() const { return mGPU; }

    [[nodiscard]] inline GPUVendor gpuVendor() const { return mGPUVendor; }
    [[nodiscard]] inline CPUArchitecture cpuArchitecture() const { return mCPUArchitecture; }

    [[nodiscard]] inline size_t device() const { return mDevice; }
    inline void setDevice(size_t d) { mDevice = d; }
    [[nodiscard]] inline size_t threadCount() const { return mThreadCount; }
    inline void setThreadCount(size_t d) { mThreadCount = d; }
    [[nodiscard]] inline size_t vectorWidth() const { return mVectorWidth; }
    inline void setVectorWidth(size_t d) { mVectorWidth = d; }

    [[nodiscard]] std::string toString() const;

    [[nodiscard]] static Target makeGeneric();
    [[nodiscard]] static Target makeSingle();

    [[nodiscard]] static Target makeCPU(size_t threads, size_t vectorWidth);
    [[nodiscard]] static Target makeCPU(CPUArchitecture arch, size_t threads, size_t vectorWidth);
    [[nodiscard]] static Target makeGPU(GPUVendor vendor, size_t device);

    [[nodiscard]] static Target pickCPU();
    [[nodiscard]] static Target pickGPU(size_t device = 0);
    [[nodiscard]] static Target pickBest();

private:
    bool mInitialized;
    bool mGPU;

    GPUVendor mGPUVendor;
    CPUArchitecture mCPUArchitecture;

    size_t mDevice;
    size_t mThreadCount;
    size_t mVectorWidth;
};

} // namespace IG