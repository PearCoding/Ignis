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

class Runtime {
    IG_CLASS_NON_COPYABLE(Runtime);
    IG_CLASS_NON_MOVEABLE(Runtime);
public:
    explicit Runtime(const RuntimeOptions& opts);
    ~Runtime();

    /// Load from file and initialize
    bool loadFromFile(const std::filesystem::path& path);
    /// Load from string and initialize
    bool loadFromString(const std::string& str);

    /// Do a single iteration in non-tracing mode
    void step();
    /// Do a single iteration in tracing mode
    void trace(const std::vector<Ray>& rays, std::vector<float>& data);
    /// Reset internal counters etc. This should be used if data (like camera orientation) has changed
    void reset();

    /// A utility function to speed up tonemapping
    /// out_pixels should be of size width*height!
    void tonemap(uint32* out_pixels, const TonemapSettings& settings);
    /// A utility function to speed up utility information from the image
    void imageinfo(const ImageInfoSettings& settings, ImageInfoOutput& output);

    /// Will resize the framebuffer, clear it and reset rendering
    void resizeFramebuffer(size_t width, size_t height);
    /// Return pointer to framebuffer
    const float* getFramebuffer(size_t aov = 0) const;
    /// Will clear all framebuffers 
    void clearFramebuffer();
    /// Will clear specific framebuffer
    void clearFramebuffer(size_t aov);

    /// Return all names of the enabled AOVs
    inline const std::vector<std::string>& aovs() const { return mTechniqueInfo.EnabledAOVs; }

    /// Return number of iterations rendered so far
    inline size_t currentIterationCount() const { return mCurrentIteration; }
    /// Return number of samples rendered so far
    inline size_t currentSampleCount() const { return mCurrentSampleCount; }

    /// Return pointer to structure containing statistics
    const Statistics* getStatistics() const;

    /// Returns the name of the loaded technique
    inline const std::string& technique() const { return mTechniqueName; }

    /// Return true if the runtime is used in tracing mode
    inline bool isTrace() const { return mIsTrace; }

    /// The target the runtime is using
    inline Target target() const { return mTarget; }

    /// Computes (approximative) number of samples per iteration. This might be off due to the internal computing of techniques
    inline size_t samplesPerIteration() const { return mTechniqueInfo.ComputeSPI(0 /* TODO: Not always the best choice */, mSamplesPerIteration); }

    /// The bounding box of the loaded scene
    inline const BoundingBox& sceneBoundingBox() const { return mDatabase.SceneBBox; }

    /// Set integer parameter in the registry. Will replace already present values
    void setParameter(const std::string& name, int value);
    /// Set number parameter in the registry. Will replace already present values
    void setParameter(const std::string& name, float value);
    /// Set 3d vector parameter in the registry. Will replace already present values
    void setParameter(const std::string& name, const Vector3f& value);
    /// Set 4d vector parameter in the registry. Will replace already present values
    void setParameter(const std::string& name, const Vector4f& value);

    /// The current framebuffer width
    inline size_t framebufferWidth() const { return mFilmWidth; }
    /// The current framebuffer height
    inline size_t framebufferHeight() const { return mFilmHeight; }

    /// The initial camera orientation the scene was loaded with. Can be used to reset in later iterations
    inline CameraOrientation initialCameraOrientation() const { return mInitialCameraOrientation; }

private:
    bool load(const std::filesystem::path& path, Parser::Scene&& scene);
    bool setup();
    void shutdown();
    bool compileShaders();
    void stepVariant(size_t variant);
    void traceVariant(const std::vector<Ray>& rays, size_t variant);

    const RuntimeOptions mOptions;

    SceneDatabase mDatabase;
    DriverInterface mLoadedInterface;
    DriverManager mManager;
    ParameterSet mParameterSet;

    size_t mDevice;
    size_t mSamplesPerIteration;
    Target mTarget;

    size_t mCurrentIteration;
    size_t mCurrentSampleCount;

    size_t mFilmWidth;
    size_t mFilmHeight;

    CameraOrientation mInitialCameraOrientation;

    bool mIsTrace;
    bool mAcquireStats;

    std::string mTechniqueName;
    TechniqueInfo mTechniqueInfo;

    std::vector<TechniqueVariant> mTechniqueVariants;
    std::vector<TechniqueVariantShaderSet> mTechniqueVariantShaderSets; // Compiled shaders
};
} // namespace IG