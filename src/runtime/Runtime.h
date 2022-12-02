#pragma once

#include "RuntimeSettings.h"
#include "RuntimeStructs.h"
#include "Statistics.h"
#include "device/Device.h"
#include "loader/Loader.h"
#include "shader/ScriptCompiler.h"
#include "table/SceneDatabase.h"

namespace IG {
namespace Parser {
class Scene;
}

struct LoaderOptions;

using AOVAccessor = Device::AOVAccessor;

class IG_LIB Runtime {
    IG_CLASS_NON_COPYABLE(Runtime);
    IG_CLASS_NON_MOVEABLE(Runtime);

public:
    explicit Runtime(const RuntimeOptions& opts);
    ~Runtime();

    /// Load from file and initialize
    [[nodiscard]] bool loadFromFile(const std::filesystem::path& path);
    /// Load from string and initialize
    /// @param str String containing valid scene description
    /// @param dir Optional directory containing external files if not given as absolute files inside the scene description
    [[nodiscard]] bool loadFromString(const std::string& str, const std::filesystem::path& dir);

    /// Do a single iteration in non-tracing mode
    void step(bool ignoreDenoiser = false);
    /// Do a single iteration in tracing mode and return values in data
    void trace(const std::vector<Ray>& rays, std::vector<float>& data);
    /// Do a single iteration in tracing mode. Output will be in the framebuffer
    void trace(const std::vector<Ray>& rays);
    /// Reset internal counters etc. This should be used if data (like camera orientation) has changed. Frame counter will NOT be reset
    void reset();

    /// A utility function to speed up tonemapping
    /// out_pixels should be of size width*height!
    void tonemap(uint32* out_pixels, const TonemapSettings& settings);
    /// A utility function to speed up utility information from the image
    ImageInfoOutput imageinfo(const ImageInfoSettings& settings);

    /// Will resize the framebuffer, clear it and reset rendering
    void resizeFramebuffer(size_t width, size_t height);
    /// Return pointer to framebuffer
    /// name == 'Color' or null returns the actual framebuffer, else the corresponding AOV will be returned
    [[nodiscard]] AOVAccessor getFramebuffer(const std::string& name) const;
    /// Will clear all framebuffers
    void clearFramebuffer();
    /// Will clear specific framebuffer
    void clearFramebuffer(const std::string& name);

    /// Return all names of the enabled AOVs
    [[nodiscard]] inline const std::vector<std::string>& aovs() const { return mTechniqueInfo.EnabledAOVs; }

    /// Return number of iterations rendered so far
    [[nodiscard]] inline size_t currentIterationCount() const { return mCurrentIteration; }
    /// Return number of samples rendered so far
    [[nodiscard]] inline size_t currentSampleCount() const { return mCurrentSampleCount; }

    /// Return pointer to structure containing statistics
    [[nodiscard]] const Statistics* getStatistics() const;

    /// Returns the name of the loaded technique
    [[nodiscard]] inline const std::string& technique() const { return mTechniqueName; }

    /// Returns the name of the loaded camera
    [[nodiscard]] inline const std::string& camera() const { return mCameraName; }

    /// Return true if the runtime is used in tracing mode
    [[nodiscard]] inline bool isTrace() const { return mOptions.IsTracer; }

    /// The target the runtime is using
    [[nodiscard]] inline const Target& target() const { return mOptions.Target; }

    /// Computes (approximative) number of samples per iteration. This might be off due to the internal computing of techniques
    [[nodiscard]] inline size_t samplesPerIteration() const { return mTechniqueInfo.ComputeSPI(0 /* TODO: Not always the best choice */, mSamplesPerIteration); }

    /// The bounding box of the loaded scene
    [[nodiscard]] inline const BoundingBox& sceneBoundingBox() const { return mDatabase.SceneBBox; }

    /// Set integer parameter in the registry. Will replace already present values
    void setParameter(const std::string& name, int value);
    /// Set number parameter in the registry. Will replace already present values
    void setParameter(const std::string& name, float value);
    /// Set 3d vector parameter in the registry. Will replace already present values
    void setParameter(const std::string& name, const Vector3f& value);
    /// Set 4d vector parameter in the registry. Will replace already present values
    void setParameter(const std::string& name, const Vector4f& value);

    /// The current framebuffer width
    [[nodiscard]] inline size_t framebufferWidth() const { return mFilmWidth; }
    /// The current framebuffer height
    [[nodiscard]] inline size_t framebufferHeight() const { return mFilmHeight; }

    /// The initial camera orientation the scene was loaded with. Can be used to reset in later iterations
    [[nodiscard]] inline CameraOrientation initialCameraOrientation() const { return mInitialCameraOrientation; }
    void setCameraOrientationParameter(const CameraOrientation& orientation);

    /// Increase frame count (only used in interactive sessions)
    inline void incFrameCount() { mCurrentFrame++; }

    [[nodiscard]] bool hasDenoiser() const;

    /// Get a list of all available techniques
    [[nodiscard]] static std::vector<std::string> getAvailableTechniqueTypes();

    /// Get a list of all available cameras
    [[nodiscard]] static std::vector<std::string> getAvailableCameraTypes();

private:
    void checkCacheDirectory();
    bool load(const std::filesystem::path& path, Parser::Scene&& scene);
    bool setup();
    void shutdown();
    bool compileShaders();
    void* compileShader(const std::string& src, const std::string& func, const std::string& name);
    void stepVariant(bool ignoreDenoiser, size_t variant, bool lastVariant);
    void traceVariant(const std::vector<Ray>& rays, size_t variant);

    const RuntimeOptions mOptions;

    SceneDatabase mDatabase;
    ParameterSet mParameterSet;
    ScriptCompiler mCompiler;

    std::unique_ptr<Device> mDevice;

    size_t mSamplesPerIteration;

    size_t mCurrentIteration;
    size_t mCurrentSampleCount;
    size_t mCurrentFrame;

    size_t mFilmWidth;
    size_t mFilmHeight;

    std::string mCameraName;
    CameraOrientation mInitialCameraOrientation;

    std::string mTechniqueName;
    TechniqueInfo mTechniqueInfo;

    std::vector<std::string> mResourceMap;

    std::vector<TechniqueVariant> mTechniqueVariants;
    std::vector<TechniqueVariantShaderSet> mTechniqueVariantShaderSets; // Compiled shaders
};
} // namespace IG