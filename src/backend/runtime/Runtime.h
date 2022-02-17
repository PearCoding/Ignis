#pragma once

#include "DebugMode.h"
#include "RuntimeStructs.h"
#include "Statistics.h"
#include "driver/DriverManager.h"
#include "loader/Loader.h"
#include "table/SceneDatabase.h"

namespace IG {
class Camera;
struct LoaderOptions;

struct RuntimeOptions {
    bool DumpShader      = false;
    bool DumpShaderFull  = false;
    bool AcquireStats    = false;
    Target DesiredTarget = Target::INVALID;
    bool RecommendCPU    = true;
    bool RecommendGPU    = true;
    uint32 Device        = 0;
    uint32 SPI           = 0; // Detect automatically
    std::string OverrideTechnique;
    std::string OverrideCamera;
    std::pair<uint32, uint32> OverrideFilmSize = { 0, 0 };
};

struct RuntimeRenderSettings {
    uint32 FilmWidth   = 800;
    uint32 FilmHeight  = 600;
    Vector3f CameraEye = Vector3f::Zero();
    Vector3f CameraDir = Vector3f::UnitZ();
    Vector3f CameraUp  = Vector3f::UnitY();
    float FOV          = 60;
    float TMin         = 0;
    float TMax         = FltMax;
};

struct Ray {
    Vector3f Origin;
    Vector3f Direction;
    Vector2f Range;
};

class Runtime {
public:
    Runtime(const std::filesystem::path& path, const RuntimeOptions& opts);
    ~Runtime();

    void setup();
    void step(const Camera& camera);
    void trace(const std::vector<Ray>& rays, std::vector<float>& data);
    void reset();

    /// A utility function to speed up tonemapping
    /// out_pixels should be of size width*height!
    void tonemap(uint32* out_pixels, const TonemapSettings& settings);
    /// A utility function to speed up utility information from the image
    void imageinfo(const ImageInfoSettings& settings, ImageInfoOutput& output);

    /// Will resize the framebuffer, clear it and reset rendering
    void resizeFramebuffer(size_t width, size_t height);
    const float* getFramebuffer(int aov = 0) const;
    // aov<0 will clear all aovs
    void clearFramebuffer(int aov = -1);
    inline const std::vector<std::string> aovs() const { return mTechniqueInfo.EnabledAOVs; }

    inline uint32 currentIterationCount() const { return mCurrentIteration; }
    inline uint32 currentSampleCount() const { return mCurrentSampleCount; }

    const Statistics* getStatistics() const;

    inline const RuntimeRenderSettings& loadedRenderSettings() const { return mLoadedRenderSettings; }

    inline DebugMode currentDebugMode() const { return mDebugMode; }
    inline void setDebugMode(DebugMode mode) { mDebugMode = mode; }
    inline bool isDebug() const { return mIsDebug; }
    inline bool isTrace() const { return mIsTrace; }

    inline Target target() const { return mTarget; }
    inline size_t samplesPerIteration() const { return mTechniqueInfo.ComputeSPI(0 /* TODO: Not always the best choice */, mSamplesPerIteration); }

    inline const BoundingBox& sceneBoundingBox() const { return mDatabase.SceneBBox; }

private:
    void shutdown();
    void compileShaders();
    void stepVariant(const Camera& camera, int variant);
    void traceVariant(const std::vector<Ray>& rays, int variant);

    bool mInit;

    const RuntimeOptions mOptions;

    SceneDatabase mDatabase;
    RuntimeRenderSettings mLoadedRenderSettings;
    DriverInterface mLoadedInterface;
    DriverManager mManager;

    size_t mDevice;
    size_t mSamplesPerIteration;
    Target mTarget;

    uint32 mCurrentIteration;
    uint32 mCurrentSampleCount;

    bool mIsTrace;
    bool mIsDebug;
    DebugMode mDebugMode;
    bool mAcquireStats;
    TechniqueInfo mTechniqueInfo;

    std::vector<TechniqueVariant> mTechniqueVariants;
    std::vector<TechniqueVariantShaderSet> mTechniqueVariantShaderSets; // Compiled shaders
};
} // namespace IG