#include "Device.h"
#include "DeviceUtils.h"
#include "IDeviceInterface.h"
#include "Logger.h"

namespace IG {
// --------------------- Device
Device::Device(const std::shared_ptr<IDeviceInterface>& device)
    : mDevice(device)
{
    if (device->target().isCPU() && device->target().vectorWidth() > 1)
        IG_LOG(L_WARNING) << "CPU device with vector width > 1 is experimental and might crash!" << std::endl;
}

Device::~Device()
{
}

Target Device::target() const { return mDevice->target(); }

size_t Device::framebufferWidth() const { return std::get<0>(mDevice->framebufferSize()); }

size_t Device::framebufferHeight() const { return std::get<1>(mDevice->framebufferSize()); }

void Device::assignScene(const SceneSettings& settings)
{
    mDevice->setCurrentSceneSettings(settings);
}

void Device::render(const TechniqueVariantShaderSet& shaderSet, const Device::RenderSettings& settings, ParameterSet* parameterSet)
{
    mDevice->updateContext(shaderSet, settings, parameterSet);
    mDevice->runDeviceShader();
}

void Device::resize(size_t width, size_t height)
{
    mDevice->resizeFramebuffer(width, height);
}

void Device::releaseAll()
{
    mDevice->releaseAllMemory();
}

Device::AOVAccessor Device::getFramebufferForHost(const std::string& name)
{
    const auto acc = mDevice->loadAOVImageForHost(name);
    return {
        .Data = acc.DataPtr
    };
}

Device::AOVAccessor Device::getFramebufferForDevice(const std::string& name)
{
    const auto acc = mDevice->loadAOVImageForDevice(name);
    return {
        .Data = acc.DataPtr
    };
}

void Device::clearAllFramebuffer()
{
    mDevice->clearAllAOVs();
}

void Device::clearFramebuffer(const std::string& name)
{
    mDevice->clearAOV(name);
}

void Device::syncFramebufferHostToDevice(const std::string& name)
{
    mDevice->mapAOVBackToDevice(name);
}

void Device::syncAllFramebufferHostToDevice()
{
    mDevice->mapAllAOVsBackToDevice();
}

size_t Device::getBufferSizeInBytes(const std::string& name)
{
    const size_t size = mDevice->loadBufferByName(name).DataSize;
    return size;
}

bool Device::copyBufferToHost(const std::string& name, void* dst, size_t maxSizeByte)
{
    const auto successful = mDevice->copyBufferToHost(name, dst, maxSizeByte);
    return successful;
}

Device::BufferAccessor Device::getBufferForDevice(const std::string& name)
{
    const auto acc = mDevice->loadBufferByName(name);
    return {
        .Data        = acc.DataPtr,
        .SizeInBytes = acc.DataSize
    };
}

const Statistics& Device::getStatistics()
{
    return mDevice->getAcquiredStatistics();
}

void Device::tonemap(uint32_t* out_pixels, const TonemapSettings& settings)
{
    const auto acc   = mDevice->loadAOVImageForDevice(settings.AOV);
    float* in_pixels = acc.DataPtr;

    uint32_t* device_out_pixels = out_pixels;
    // Allocate a new buffer and map back to host
    if (mDevice->isGPU())
        device_out_pixels = (uint32_t*)mDevice->requestBuffer("__internal_tonemap_output", acc.Width * acc.Height * sizeof(uint32_t), 0).DataPtr;

    mDevice->runTonemapShader(in_pixels, device_out_pixels, settings);

    if (mDevice->isGPU())
        mDevice->copyBufferToHost("__internal_tonemap_output", out_pixels, acc.Width * acc.Height * sizeof(uint32_t));
}

ImageInfoOutput Device::imageinfo(const ImageInfoSettings& settings)
{
    const auto acc   = mDevice->loadAOVImageForDevice(settings.AOV);
    float* in_pixels = acc.DataPtr;

    return mDevice->runImageInfoShader(in_pixels, settings);
}

void Device::bake(const ShaderOutput<void*>& shader, const std::vector<std::string>* resource_map, float* output)
{
    mDevice->runBakeShader(shader, resource_map, output);
}

void Device::runPass(const ShaderOutput<void*>& shader, void* userData)
{
    mDevice->runPassShader(shader, userData);
}
} // namespace IG
