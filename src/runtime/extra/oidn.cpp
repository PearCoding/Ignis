#include "Logger.h"
#include "device/IRenderDevice.h"

#ifdef IG_HAS_DENOISER
#include "OpenImageDenoise/oidn.hpp"

namespace IG {
static void errorFunc(void* userPtr, oidn::Error code, const char* message)
{
    IG_UNUSED(userPtr);

    if (code == oidn::Error::None)
        IG_LOG(IG::L_INFO) << "OIDN: " << message << std::endl;
    else if(strcmp(message, "driver shutting down") != 0 /* Ignore this message */)
        IG_LOG(IG::L_ERROR) << "OIDN: " << message << std::endl;
}

#if OIDN_VERSION_MAJOR >= 2
class OIDNContext {
public:
    OIDNContext()
        : mPrefilter(false) // TODO: Should be a user parameter?
        , mWidth(0)
        , mHeight(0)
        , mDeviceType()
    {
        mDevice = oidn::newDevice(oidn::DeviceType::Default);

        mDevice.setErrorFunction(errorFunc);
        mDevice.commit();

        const int versionMajor = mDevice.get<int>("versionMajor");
        const int versionMinor = mDevice.get<int>("versionMinor");
        const int versionPatch = mDevice.get<int>("versionPatch");

        mDeviceType = mDevice.get<oidn::DeviceType>("type");
        std::string name;
        switch (mDeviceType) {
        default:
        case oidn::DeviceType::Default:
            name = "Default";
            break;
        case oidn::DeviceType::CPU:
            name = "CPU";
            break;
        case oidn::DeviceType::CUDA:
            name = "CUDA";
            break;
        case oidn::DeviceType::HIP:
            name = "HIP";
            break;
        case oidn::DeviceType::SYCL:
            name = "SYCL";
            break;
        }
        IG_LOG(IG::L_INFO) << "Using OpenImageDenoise " << versionMajor << "." << versionMinor << "." << versionPatch << " with device " << name << std::endl;
    }

    inline bool isSameDevice(Target target)
    {
        switch (mDeviceType) {
        default:
        case oidn::DeviceType::Default:
        case oidn::DeviceType::CPU:
            return target.isCPU();
        case oidn::DeviceType::CUDA:
            if (!target.isGPU())
                return false;
            if (target.gpuArchitecture() == GPUArchitecture::Nvidia)
                return true; // TODO: Device number!
            return false;
        case oidn::DeviceType::HIP:
            if (!target.isGPU())
                return false;
            if (target.gpuArchitecture() == GPUArchitecture::AMD_HSA)
                return true;         // TODO: Device number!
            return false;
        case oidn::DeviceType::SYCL: // TODO: Sycl is not necessarily Intel only
            if (!target.isGPU())
                return false;
            if (target.gpuArchitecture() == GPUArchitecture::Intel)
                return true; // TODO: Device number!
            return false;
        }
    }

    inline void filter(IRenderDevice* device)
    {
        if (isSameDevice(device->target()))
            filterDevice(device);
        else
            filterHost(device);
    }

    inline void filterHost(IRenderDevice* device)
    {
        const auto color  = device->getFramebufferForHost({});
        const auto normal = device->getFramebufferForHost("Normals");
        const auto albedo = device->getFramebufferForHost("Albedo");
        const auto output = device->getFramebufferForHost("Denoised");

        IG_ASSERT(color.Data, "Expected valid color data for denoiser");
        IG_ASSERT(normal.Data, "Expected valid normal data for denoiser");
        IG_ASSERT(albedo.Data, "Expected valid albedo data for denoiser");
        IG_ASSERT(output.Data, "Expected valid output data for denoiser");

        const size_t width  = device->framebufferWidth();
        const size_t height = device->framebufferHeight();

        if (mWidth != width || mHeight != height)
            setupHost(width, height, device->isInteractive());

        const size_t framebufferSize = 3 * width * height;
        mColorBuffer.write(0, sizeof(float) * framebufferSize, color.Data);
        mNormalBuffer.write(0, sizeof(float) * framebufferSize, normal.Data);
        mAlbedoBuffer.write(0, sizeof(float) * framebufferSize, albedo.Data);

        if (mPrefilter) {
            mNormalFilter.execute();
            mAlbedoFilter.execute();
        }
        mMainFilter.execute();

        mOutputBuffer.read(0, sizeof(float) * framebufferSize, output.Data);
    }

    inline void filterDevice(IRenderDevice* device)
    {
        const auto color  = device->getFramebufferForDevice({});
        const auto normal = device->getFramebufferForDevice("Normals");
        const auto albedo = device->getFramebufferForDevice("Albedo");
        const auto output = device->getFramebufferForDevice("Denoised");

        IG_ASSERT(color.Data, "Expected valid color data for denoiser");
        IG_ASSERT(normal.Data, "Expected valid normal data for denoiser");
        IG_ASSERT(albedo.Data, "Expected valid albedo data for denoiser");
        IG_ASSERT(output.Data, "Expected valid output data for denoiser");

        const size_t width  = device->framebufferWidth();
        const size_t height = device->framebufferHeight();

        if (mWidth != width || mHeight != height)
            setupDevice(color.Data, normal.Data, albedo.Data, output.Data, width, height, device->isInteractive());

        if (mPrefilter) {
            mNormalFilter.execute();
            mAlbedoFilter.execute();
        }
        mMainFilter.execute();
    }

private:
    inline void setupHost(size_t width, size_t height, bool isInteractive)
    {
        const size_t framebufferSize = 3 * width * height;

        mColorBuffer  = mDevice.newBuffer(sizeof(float) * framebufferSize);
        mNormalBuffer = mDevice.newBuffer(sizeof(float) * framebufferSize);
        mAlbedoBuffer = mDevice.newBuffer(sizeof(float) * framebufferSize);
        mOutputBuffer = mDevice.newBuffer(sizeof(float) * framebufferSize);

        setup(width, height, isInteractive);
    }

    inline void setupDevice(const float* color, const float* normal, const float* albedo, float* output,
                            size_t width, size_t height, bool isInteractive)
    {
        const size_t framebufferSize = 3 * width * height;

        mColorBuffer  = mDevice.newBuffer(const_cast<float*>(color), sizeof(float) * framebufferSize);
        mNormalBuffer = mDevice.newBuffer(const_cast<float*>(normal), sizeof(float) * framebufferSize);
        mAlbedoBuffer = mDevice.newBuffer(const_cast<float*>(albedo), sizeof(float) * framebufferSize);
        mOutputBuffer = mDevice.newBuffer(output, sizeof(float) * framebufferSize);

        setup(width, height, isInteractive);
    }

    inline void setup(size_t width, size_t height, bool isInteractive)
    {
        // Main
        mMainFilter = mDevice.newFilter("RT");
        mMainFilter.setImage("color", mColorBuffer, oidn::Format::Float3, width, height);
        mMainFilter.setImage("normal", mNormalBuffer, oidn::Format::Float3, width, height);
        mMainFilter.setImage("albedo", mAlbedoBuffer, oidn::Format::Float3, width, height);
        mMainFilter.setImage("output", mOutputBuffer, oidn::Format::Float3, width, height);

        if (isInteractive)
            mMainFilter.set("quality", oidn::Quality::Balanced);

        mMainFilter.set("hdr", true);
        if (mPrefilter)
            mMainFilter.set("cleanAux", true);
        mMainFilter.commit();

        if (mPrefilter) {
            // Normal
            mNormalFilter = mDevice.newFilter("RT");
            mNormalFilter.setImage("normal", mNormalBuffer, oidn::Format::Float3, width, height);
            mNormalFilter.setImage("output", mNormalBuffer, oidn::Format::Float3, width, height);

            if (isInteractive)
                mNormalFilter.set("quality", oidn::Quality::Balanced);

            mNormalFilter.commit();

            // Albedo
            mAlbedoFilter = mDevice.newFilter("RT");
            mAlbedoFilter.setImage("albedo", mAlbedoBuffer, oidn::Format::Float3, width, height);
            mAlbedoFilter.setImage("output", mAlbedoBuffer, oidn::Format::Float3, width, height);

            if (isInteractive)
                mAlbedoFilter.set("quality", oidn::Quality::Balanced);

            mAlbedoFilter.commit();
        }

        mWidth  = width;
        mHeight = height;
    }

    const bool mPrefilter;
    size_t mWidth;
    size_t mHeight;
    oidn::DeviceType mDeviceType;
    oidn::DeviceRef mDevice;

    oidn::BufferRef mColorBuffer;
    oidn::BufferRef mNormalBuffer;
    oidn::BufferRef mAlbedoBuffer;
    oidn::BufferRef mOutputBuffer;

    oidn::FilterRef mMainFilter;
    oidn::FilterRef mNormalFilter;
    oidn::FilterRef mAlbedoFilter;
};
#else
// Old, CPU only version
class OIDNContext {
public:
    OIDNContext()
        : mPrefilter(false) // TODO: Should be a user parameter?
        , mWidth(0)
        , mHeight(0)
    {
        mDevice = oidn::newDevice(oidn::DeviceType::CPU);

        mDevice.setErrorFunction(errorFunc);
        mDevice.commit();

        const int versionMajor = mDevice.get<int>("versionMajor");
        const int versionMinor = mDevice.get<int>("versionMinor");
        const int versionPatch = mDevice.get<int>("versionPatch");
        IG_LOG(IG::L_INFO) << "Using OpenImageDenoise " << versionMajor << "." << versionMinor << "." << versionPatch << std::endl;
    }

    inline void filter(IRenderDevice* device)
    {
        const auto color  = device->getFramebufferForHost({});
        const auto normal = device->getFramebufferForHost("Normals");
        const auto albedo = device->getFramebufferForHost("Albedo");
        const auto output = device->getFramebufferForHost("Denoised");

        IG_ASSERT(color.Data, "Expected valid color data for denoiser");
        IG_ASSERT(normal.Data, "Expected valid normal data for denoiser");
        IG_ASSERT(albedo.Data, "Expected valid albedo data for denoiser");
        IG_ASSERT(output.Data, "Expected valid output data for denoiser");

        const size_t width  = device->framebufferWidth();
        const size_t height = device->framebufferHeight();

        if (mWidth != width || mHeight != height)
            setup(color.Data, normal.Data, albedo.Data, output.Data, width, height);

        if (mPrefilter) {
            mNormalFilter.execute();
            mAlbedoFilter.execute();
        }
        mMainFilter.execute();
    }

private:
    inline void setup(const float* color, const float* normal, const float* albedo, float* output,
                      size_t width, size_t height)
    {
        const size_t framebufferSize = 3 * width * height;

        mColorBuffer  = mDevice.newBuffer(const_cast<float*>(color), sizeof(float) * framebufferSize);
        mNormalBuffer = mDevice.newBuffer(const_cast<float*>(normal), sizeof(float) * framebufferSize);
        mAlbedoBuffer = mDevice.newBuffer(const_cast<float*>(albedo), sizeof(float) * framebufferSize);
        mOutputBuffer = mDevice.newBuffer(output, sizeof(float) * framebufferSize);

        // Main
        mMainFilter = mDevice.newFilter("RT");
        mMainFilter.setImage("color", mColorBuffer, oidn::Format::Float3, width, height);
        mMainFilter.setImage("normal", mNormalBuffer, oidn::Format::Float3, width, height);
        mMainFilter.setImage("albedo", mAlbedoBuffer, oidn::Format::Float3, width, height);
        mMainFilter.setImage("output", mOutputBuffer, oidn::Format::Float3, width, height);

        mMainFilter.set("hdr", true);
        if (mPrefilter)
            mMainFilter.set("cleanAux", true);
        mMainFilter.commit();

        if (mPrefilter) {
            // Normal
            mNormalFilter = mDevice.newFilter("RT");
            mNormalFilter.setImage("normal", mNormalBuffer, oidn::Format::Float3, width, height);
            mNormalFilter.setImage("output", mNormalBuffer, oidn::Format::Float3, width, height);
            mNormalFilter.commit();

            // Albedo
            mAlbedoFilter = mDevice.newFilter("RT");
            mAlbedoFilter.setImage("albedo", mAlbedoBuffer, oidn::Format::Float3, width, height);
            mAlbedoFilter.setImage("output", mAlbedoBuffer, oidn::Format::Float3, width, height);
            mAlbedoFilter.commit();
        }

        mWidth  = width;
        mHeight = height;
    }

    const bool mPrefilter;
    size_t mWidth;
    size_t mHeight;
    oidn::DeviceRef mDevice;

    oidn::BufferRef mColorBuffer;
    oidn::BufferRef mNormalBuffer;
    oidn::BufferRef mAlbedoBuffer;
    oidn::BufferRef mOutputBuffer;

    oidn::FilterRef mMainFilter;
    oidn::FilterRef mNormalFilter;
    oidn::FilterRef mAlbedoFilter;
};
#endif

// Will be exposed to the device and used in Device.cpp
// TODO: This can be handled waaaay cleaner
IG_EXPORT void ignis_denoise(IRenderDevice* device)
{
    using namespace oidn;

    static auto ctx = OIDNContext();
    ctx.filter(device);
}
} // namespace IG

#endif