#pragma once

#include "RuntimeStructs.h"
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
    const IG::Ray* rays     = nullptr; // If non-null, width contains the number of rays and height is set to 1
    IG::uint32 device       = 0;
    IG::uint32 spi          = 8;
    size_t work_width       = 0;
    size_t work_height      = 0;
    bool framebuffer_locked = false;
    IG::uint32 debug_mode   = 0; // Only useful for debug integrator
};

using DriverRenderFunction            = void (*)(const IG::TechniqueVariantShaderSet&, const DriverRenderSettings&, IG::uint32);
using DriverShutdownFunction          = void (*)();
using DriverSetupFunction             = void (*)(const DriverSetupSettings&);
using DriverResizeFramebufferFunction = void (*)(size_t, size_t);
using DriverGetFramebufferFunction    = const float* (*)(int);
using DriverClearFramebufferFunction  = void (*)(int);
using DriverGetStatisticsFunction     = const IG::Statistics* (*)();

using DriverTonemapFunction   = void (*)(int, uint32_t*, const IG::TonemapSettings&);
using DriverImageInfoFunction = void (*)(int, const IG::ImageInfoSettings&, IG::ImageInfoOutput&);

struct DriverInterface {
    IG::uint32 MajorVersion;
    IG::uint32 MinorVersion;
    const char* Name;
    IG::Target Target;
    DriverSetupFunction SetupFunction;
    DriverShutdownFunction ShutdownFunction;
    DriverRenderFunction RenderFunction;
    DriverResizeFramebufferFunction ResizeFramebufferFunction;
    DriverGetFramebufferFunction GetFramebufferFunction;
    DriverClearFramebufferFunction ClearFramebufferFunction;
    DriverGetStatisticsFunction GetStatisticsFunction;
    DriverTonemapFunction TonemapFunction;
    DriverImageInfoFunction ImageInfoFunction;
};