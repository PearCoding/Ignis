#pragma once

#include "RuntimeStructs.h"
#include "device/Target.h"
#include "technique/TechniqueInfo.h"
#include "technique/TechniqueVariant.h"
#include <vector>

namespace IG {

class IDeviceInterface;
class Statistics;
struct SceneDatabase;

class IG_LIB Device {
public:
    struct SetupSettings {
        IG::Target Target;
        bool AcquireStats = false;
        bool DebugTrace   = false;
    };

    struct SceneSettings {
        SceneDatabase* database                       = nullptr;
        const std::vector<std::string>* aov_map       = nullptr;
        const std::vector<std::string>* resource_map  = nullptr;
        const std::vector<int32>* entity_per_material = nullptr; // Contains number of entities per unique material
    };

    struct RenderSettings {
        const Ray* rays  = nullptr; // If non-null, width contains the number of rays and height is set to 1
        size_t spi       = 8;
        size_t width     = 0;
        size_t height    = 0;
        size_t iteration = 0;
        size_t frame     = 0;
        size_t user_seed = 0;
        TechniqueVariantInfo info;
    };

    struct AOVAccessor {
        float* Data;
    };

    struct BufferAccessor {
        void* Data;
        size_t SizeInBytes;
    };

    explicit Device(const std::shared_ptr<IDeviceInterface>& device);
    virtual ~Device();

    void assignScene(const SceneSettings& settings);
    void render(const TechniqueVariantShaderSet& shader_set, const RenderSettings& settings, ParameterSet* parameter_set);
    void resize(size_t width, size_t height);

    void releaseAll();

    [[nodiscard]] Target target() const;
    [[nodiscard]] size_t framebufferWidth() const;
    [[nodiscard]] size_t framebufferHeight() const;

    [[nodiscard]] AOVAccessor getFramebufferForHost(const std::string& name);
    [[nodiscard]] AOVAccessor getFramebufferForDevice(const std::string& name);
    void clearFramebuffer(const std::string& name);
    void clearAllFramebuffer();
    void syncFramebufferHostToDevice(const std::string& name);
    void syncAllFramebufferHostToDevice();

    [[nodiscard]] size_t getBufferSizeInBytes(const std::string& name);
    [[nodiscard]] bool copyBufferToHost(const std::string& name, void* buffer, size_t maxSizeByte);
    [[nodiscard]] BufferAccessor getBufferForDevice(const std::string& name);

    [[nodiscard]] const Statistics& getStatistics();

    void tonemap(uint32_t*, const TonemapSettings&);
    [[nodiscard]] ImageInfoOutput imageinfo(const ImageInfoSettings&);
    void bake(const ShaderOutput<void*>& shader, const std::vector<std::string>* resource_map, float* output);

    void runPass(const ShaderOutput<void*>& shader, void* userData);

    [[nodiscard]] inline IDeviceInterface* internalDevice() const { return mDevice.get(); }

private:
    std::shared_ptr<IDeviceInterface> mDevice;
};
} // namespace IG