#pragma once

#include "Target.h"
#include "loader/TechniqueVariant.h"
#include <vector>

namespace IG {
struct SceneDatabase;
struct Ray;
class Statistics;
class Logger;
} // namespace IG

// Not in namespace IG
struct DriverSetupSettings {
    IG::uint32 framebuffer_width  = 0;
    IG::uint32 framebuffer_height = 0;
    IG::SceneDatabase* database   = nullptr;
    bool acquire_stats            = false;
    size_t aov_count              = false;

    IG::Logger* logger = nullptr;
};

struct DriverRenderSettings {
    float eye[3];
    float dir[3];
    float up[3];
    float right[3];
    float width;
    float height;
    float tmin;
    float tmax;
    const IG::Ray* rays; // If non-null, width contains the number of rays and height is set to 1
    IG::uint32 device;
    IG::uint32 spi;
    IG::uint32 max_path_length;
    IG::uint32 debug_mode; // Only useful for debug integrator
};

using DriverRenderFunction           = void (*)(const DriverRenderSettings*, IG::uint32);
using DriverShutdownFunction         = void (*)();
using DriverSetupFunction            = void (*)(const DriverSetupSettings*);
using DriverSetShaderSet             = void (*)(const IG::TechniqueVariantShaderSet& shaderSet);
using DriverGetFramebufferFunction   = const float* (*)(int);
using DriverClearFramebufferFunction = void (*)(int);
using DriverGetStatisticsFunction    = const IG::Statistics* (*)();

using DriverTonemapFunction = void (*)(int, int, uint32_t*, int, float, float, float);

struct DriverInterface {
    IG::uint32 MajorVersion;
    IG::uint32 MinorVersion;
    const char* Name;
    IG::Target Target;
    DriverSetupFunction SetupFunction;
    DriverShutdownFunction ShutdownFunction;
    DriverRenderFunction RenderFunction;
    DriverSetShaderSet SetShaderSetFunction;
    DriverGetFramebufferFunction GetFramebufferFunction;
    DriverClearFramebufferFunction ClearFramebufferFunction;
    DriverGetStatisticsFunction GetStatisticsFunction;
    DriverTonemapFunction TonemapFunction;
};