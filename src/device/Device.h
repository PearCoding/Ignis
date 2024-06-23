#pragma once

#include "device/IRenderDevice.h"

namespace IG {
class Device : public IRenderDevice {
public:
    explicit Device(const SetupSettings& settings);
    virtual ~Device();

    void assignScene(const SceneSettings& settings) override;
    void render(const TechniqueVariantShaderSet& shader_set, const RenderSettings& settings, ParameterSet* parameter_set) override;
    void resize(size_t width, size_t height) override;

    void releaseAll() override;

    [[nodiscard]] Target target() const override;
    [[nodiscard]] size_t framebufferWidth() const override;
    [[nodiscard]] size_t framebufferHeight() const override;
    [[nodiscard]] bool isInteractive() const override;

    [[nodiscard]] AOVAccessor getFramebufferForHost(const std::string& name) override;
    [[nodiscard]] AOVAccessor getFramebufferForDevice(const std::string& name) override;
    void clearFramebuffer(const std::string& name) override;
    void clearAllFramebuffer() override;

    [[nodiscard]] size_t getBufferSizeInBytes(const std::string& name) override;
    [[nodiscard]] bool copyBufferToHost(const std::string& name, void* buffer, size_t maxSizeByte) override;
    [[nodiscard]] BufferAccessor getBufferForDevice(const std::string& name) override;

    [[nodiscard]] const Statistics* getStatistics() override;

    void tonemap(uint32_t*, const TonemapSettings&) override;
    [[nodiscard]] GlareOutput evaluateGlare(uint32_t*, const GlareSettings&) override;
    [[nodiscard]] ImageInfoOutput imageinfo(const ImageInfoSettings&) override;
    void bake(const ShaderOutput<void*>& shader, const std::vector<std::string>* resource_map, float* output) override;

    void runPass(const ShaderOutput<void*>& shader) override;
};
} // namespace IG