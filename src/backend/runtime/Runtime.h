#pragma once

#include "RuntimeStructs.h"
#include "Statistics.h"
#include "driver/DriverManager.h"
#include "loader/Loader.h"
#include "table/SceneDatabase.h"

namespace IG {
namespace Parser {
    class Scene;
}

struct LoaderOptions;

struct RuntimeOptions {
    bool IsTracer        = false;
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

    std::filesystem::path ModulePath = std::filesystem::current_path(); // Optional path to modules
};

struct RuntimeRenderSettings {
    uint32 FilmWidth  = 800;
    uint32 FilmHeight = 600;
};

struct Ray {
    Vector3f Origin;
    Vector3f Direction;
    Vector2f Range;
};

class Runtime {
    IG_CLASS_NON_COPYABLE(Runtime);
    IG_CLASS_NON_MOVEABLE(Runtime);
public:
    Runtime(const RuntimeOptions& opts);
    ~Runtime();

    bool loadFromFile(const std::filesystem::path& path);
    bool loadFromString(const std::string& str);

    void step();
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
    inline const std::vector<std::string>& aovs() const { return mTechniqueInfo.EnabledAOVs; }

    inline uint32 currentIterationCount() const { return mCurrentIteration; }
    inline uint32 currentSampleCount() const { return mCurrentSampleCount; }

    const Statistics* getStatistics() const;

    inline bool isDebug() const { return mIsDebug; }
    inline bool isTrace() const { return mIsTrace; }

    inline Target target() const { return mTarget; }
    inline size_t samplesPerIteration() const { return mTechniqueInfo.ComputeSPI(0 /* TODO: Not always the best choice */, mSamplesPerIteration); }

    inline const BoundingBox& sceneBoundingBox() const { return mDatabase.SceneBBox; }

    void setParameter(const std::string& name, int value);
    void setParameter(const std::string& name, float value);
    void setParameter(const std::string& name, const Vector3f& value);
    void setParameter(const std::string& name, const Vector4f& value);

    inline size_t framebufferWidth() const { return mFilmWidth; }
    inline size_t framebufferHeight() const { return mFilmHeight; }

    inline CameraOrientation initialCameraOrientation() const { return mInitialCameraOrientation; }

private:
    bool load(const std::filesystem::path& path, Parser::Scene&& scene);
    bool setup();
    void shutdown();
    bool compileShaders();
    void stepVariant(int variant);
    void traceVariant(const std::vector<Ray>& rays, int variant);

    const RuntimeOptions mOptions;

    SceneDatabase mDatabase;
    DriverInterface mLoadedInterface;
    DriverManager mManager;
    ParameterSet mParameterSet;

    size_t mDevice;
    size_t mSamplesPerIteration;
    Target mTarget;

    uint32 mCurrentIteration;
    uint32 mCurrentSampleCount;

    size_t mFilmWidth;
    size_t mFilmHeight;

    CameraOrientation mInitialCameraOrientation;

    bool mIsTrace;
    bool mIsDebug;
    bool mAcquireStats;
    TechniqueInfo mTechniqueInfo;

    std::vector<TechniqueVariant> mTechniqueVariants;
    std::vector<TechniqueVariantShaderSet> mTechniqueVariantShaderSets; // Compiled shaders
};
} // namespace IG