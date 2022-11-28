#pragma once

#include "RuntimeStructs.h"
#include "Target.h"
#include "loader/TechniqueInfo.h"
#include "loader/TechniqueVariant.h"
#include <vector>

namespace IG {

class Statistics;
struct SceneDatabase;

class Device {
    IG_CLASS_NON_COPYABLE(Device);

public:
    struct SetupSettings {
        Target target;
        size_t framebuffer_width                     = 0;
        size_t framebuffer_height                    = 0;
        SceneDatabase* database                      = nullptr;
        bool acquire_stats                           = false;
        const std::vector<std::string>* aov_map      = nullptr;
        const std::vector<std::string>* resource_map = nullptr;
    };

    struct RenderSettings {
        const Ray* rays    = nullptr; // If non-null, width contains the number of rays and height is set to 1
        size_t spi         = 8;
        size_t work_width  = 0;
        size_t work_height = 0;
        size_t iteration   = 0;
        size_t frame       = 0;
        TechniqueVariantInfo info;
        bool apply_denoiser = false;
    };

    struct AOVAccessor {
        float* Data;
        size_t IterationCount;
    };

    Device(const SetupSettings& settings);
    ~Device();

    void render(const TechniqueVariantShaderSet& shader_set, const RenderSettings& settings, const ParameterSet* parameter_set);
    void resize(size_t width, size_t height);

    AOVAccessor getFramebuffer(const std::string& name);
    void clearFramebuffer(const std::string& name);
    void clearAllFramebuffer();

    const Statistics* getStatistics();

    void tonemap(uint32_t*, const TonemapSettings&);
    void evaluateGlare(uint32_t*, const GlareSettings&);
    ImageInfoOutput imageinfo(const ImageInfoSettings&);
};
} // namespace IG