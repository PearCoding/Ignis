#include "OpenImageDenoise/oidn.hpp"
#include "Logger.h"

static void errorFunc(void* userPtr, oidn::Error code, const char* message)
{
    IG_UNUSED(userPtr);
    IG_UNUSED(code);

    IG_LOG(IG::L_ERROR) << "OIDN: " << message << std::endl;
}

void ignis_denoise(const float* color, const float* normal, const float* albedo, const float* depth, float* output,
                   size_t width, size_t height, size_t iter)
{
    IG_UNUSED(depth);

    if (iter == 0)
        return;

    using namespace oidn;

    static auto device     = newDevice(DeviceType::CPU); // TODO: Support new stuff in the future
    static bool first_time = true;

    if (first_time) {
        device.setErrorFunction(errorFunc);
        device.commit();
        first_time = false;
    }

    const size_t framebufferSize = 3 * width * height;

    auto colorBuffer  = device.newBuffer(const_cast<float*>(color), sizeof(float) * framebufferSize);
    auto normalBuffer = device.newBuffer(const_cast<float*>(normal), sizeof(float) * framebufferSize);
    auto albedoBuffer = device.newBuffer(const_cast<float*>(albedo), sizeof(float) * framebufferSize);
    // auto depthBuffer  = device.newBuffer(const_cast<float*>(depth), sizeof(float) * framebufferSize);
    auto outputBuffer = device.newBuffer(output, sizeof(float) * framebufferSize);

    auto filter = device.newFilter("RT");
    filter.setImage("color", colorBuffer, Format::Float3, width, height);
    filter.setImage("normal", normalBuffer, Format::Float3, width, height);
    filter.setImage("albedo", albedoBuffer, Format::Float3, width, height);
    filter.setImage("output", outputBuffer, Format::Float3, width, height);

    filter.set("hdr", true);
    filter.set("inputScale", 1.0f / iter);

    filter.commit();

    filter.execute();
}