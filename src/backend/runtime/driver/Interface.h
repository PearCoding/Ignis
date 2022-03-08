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
    const char* driver_filename = nullptr;
    size_t framebuffer_width    = 0;
    size_t framebuffer_height   = 0;
    IG::SceneDatabase* database = nullptr;
    bool acquire_stats          = false;
    size_t aov_count            = false;

    IG::Logger* logger = nullptr;
};

struct DriverRenderSettings {
    const IG::Ray* rays     = nullptr; // If non-null, width contains the number of rays and height is set to 1
    size_t device           = 0;
    size_t spi              = 8;
    size_t work_width       = 0;
    size_t work_height      = 0;
    bool framebuffer_locked = false;
};

using DriverRenderFunction            = void (*)(const IG::TechniqueVariantShaderSet&, const DriverRenderSettings&, const IG::ParameterSet*, size_t);
using DriverShutdownFunction          = void (*)();
using DriverSetupFunction             = void (*)(const DriverSetupSettings&);
using DriverResizeFramebufferFunction = void (*)(size_t, size_t);
using DriverGetFramebufferFunction    = const float* (*)(size_t);
using DriverClearFramebufferFunction  = void (*)(int);
using DriverGetStatisticsFunction     = const IG::Statistics* (*)();

using DriverTonemapFunction   = void (*)(size_t, uint32_t*, const IG::TonemapSettings&);
using DriverImageInfoFunction = void (*)(size_t, const IG::ImageInfoSettings&, IG::ImageInfoOutput&);

using DriverCompileSourceFunction = void* (*)(const char*, const char*, const char*);

struct DriverInterface {
    IG::uint32 MajorVersion;
    IG::uint32 MinorVersion;
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
    DriverCompileSourceFunction CompileSourceFunction;
};