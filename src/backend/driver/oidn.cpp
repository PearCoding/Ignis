#include "OpenImageDenoise/oidn.hpp"
#include "Logger.h"

static void errorFunc(void* userPtr, oidn::Error code, const char* message)
{
    IG_UNUSED(userPtr);
    IG_UNUSED(code);

    IG_LOG(IG::L_ERROR) << "OIDN: " << message << std::endl;
}

class OIDNContext {
public:
    OIDNContext()
        : mPrefilter(false) // TODO: Should be a user parameter?
        , mWidth(0)
        , mHeight(0)
    {
        mDevice = oidn::newDevice(oidn::DeviceType::Default);

        mDevice.setErrorFunction(errorFunc);
        mDevice.commit();

        const int versionMajor = mDevice.get<int>("versionMajor");
        const int versionMinor = mDevice.get<int>("versionMinor");
        const int versionPatch = mDevice.get<int>("versionPatch");
        IG_LOG(IG::L_INFO) << "Using OpenImageDenoise " << versionMajor << "." << versionMinor << "." << versionPatch << std::endl;
    }

    inline void filter(const float* color, const float* normal, const float* albedo, float* output,
                       size_t width, size_t height)
    {
        if (mWidth != width || mHeight != height)
            setup(color, normal, albedo, output, width, height);

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

void ignis_denoise(const float* color, const float* normal, const float* albedo, const float* depth, float* output,
                   size_t width, size_t height, size_t iter)
{
    IG_UNUSED(depth);

    if (iter == 0)
        return;

    using namespace oidn;

    static auto device = OIDNContext();
    device.filter(color, normal, albedo, output, width, height);
}