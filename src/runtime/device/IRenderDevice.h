#pragma once

#include "RuntimeStructs.h"
#include "device/Target.h"
#include "technique/TechniqueInfo.h"
#include "technique/TechniqueVariant.h"
#include <vector>

namespace IG {

class Statistics;
struct SceneDatabase;

class IG_LIB IRenderDevice {
public:
    struct SetupSettings {
        Target target;
        bool AcquireStats  = false;
        bool DebugTrace    = false;
        bool IsInteractive = false;
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
        bool denoise = false;
    };

    struct AOVAccessor {
        float* Data;
        size_t IterationCount;
    };

    virtual ~IRenderDevice() = default;

    virtual void assignScene(const SceneSettings& settings)                                                                       = 0;
    virtual void render(const TechniqueVariantShaderSet& shader_set, const RenderSettings& settings, ParameterSet* parameter_set) = 0;
    virtual void resize(size_t width, size_t height)                                                                              = 0;

    virtual void releaseAll() = 0;

    [[nodiscard]] virtual Target target() const            = 0;
    [[nodiscard]] virtual size_t framebufferWidth() const  = 0;
    [[nodiscard]] virtual size_t framebufferHeight() const = 0;
    [[nodiscard]] virtual bool isInteractive() const       = 0;

    [[nodiscard]] virtual AOVAccessor getFramebufferForHost(const std::string& name)   = 0;
    [[nodiscard]] virtual AOVAccessor getFramebufferForDevice(const std::string& name) = 0;
    virtual void clearFramebuffer(const std::string& name)                             = 0;
    virtual void clearAllFramebuffer()                                                 = 0;

    [[nodiscard]] virtual const Statistics* getStatistics() = 0;

    virtual void tonemap(uint32_t*, const TonemapSettings&)                                                           = 0;
    [[nodiscard]] virtual GlareOutput evaluateGlare(uint32_t*, const GlareSettings&)                                  = 0;
    [[nodiscard]] virtual ImageInfoOutput imageinfo(const ImageInfoSettings&)                                         = 0;
    virtual void bake(const ShaderOutput<void*>& shader, const std::vector<std::string>* resource_map, float* output) = 0;

    
};
} // namespace IG