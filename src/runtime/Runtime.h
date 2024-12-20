#pragma once

#include "ParameterDescSet.h"
#include "ParameterSet.h"
#include "RenderPass.h"
#include "RuntimeSettings.h"
#include "RuntimeStructs.h"
#include "Statistics.h"
#include "camera/CameraOrientation.h"
#include "device/ICompilerDevice.h"
#include "device/IRenderDevice.h"
#include "extra/OIDN.h"
#include "loader/Loader.h"
#include "shader/ScriptCompiler.h"
#include "table/SceneDatabase.h"

namespace IG {

struct LoaderOptions;
class Scene;

using AOVAccessor    = IRenderDevice::AOVAccessor;
using BufferAccessor = IRenderDevice::BufferAccessor;

class IG_LIB Runtime {
    IG_CLASS_NON_COPYABLE(Runtime);
    IG_CLASS_NON_MOVEABLE(Runtime);

public:
    using Timepoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    explicit Runtime(const RuntimeOptions& opts);
    ~Runtime();

    /// Load from file and initialize
    [[nodiscard]] bool loadFromFile(const Path& path);

    /// Load from string and initialize
    /// @param str String containing valid scene description
    /// @param dir Optional directory containing external files if not given as absolute files inside the scene description
    [[nodiscard]] bool loadFromString(const std::string& str, const Path& dir);

    /// Load from an already present scene and initialize
    /// @param scene Valid scene
    [[nodiscard]] bool loadFromScene(const Scene* scene);

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
    /// Return pointer to framebuffer. The returned buffer is host only. This might trigger a device -> host copy operation.
    /// name == 'Color' or empty returns the actual framebuffer, else the corresponding AOV will be returned.
    /// @note If the same device is used to work with the framebuffer use getFramebufferForDevice instead.
    [[nodiscard]] AOVAccessor getFramebufferForHost(const std::string& name) const;
    /// Return pointer to framebuffer. The returned buffer is device specific.
    /// name == 'Color' or empty returns the actual framebuffer, else the corresponding AOV will be returned.
    /// @note The host can not access the data pointed by the buffer without ensuring a device -> host map.
    [[nodiscard]] AOVAccessor getFramebufferForDevice(const std::string& name) const;
    /// Will clear all framebuffers
    void clearFramebuffer();
    /// Will clear specific framebuffer
    void clearFramebuffer(const std::string& name);

    [[nodiscard]] size_t getBufferSizeInBytes(const std::string& name) const;
    [[nodiscard]] BufferAccessor getBufferForDevice(const std::string& name) const;
    bool copyBufferToHost(const std::string& name, void* dst, size_t maxSizeInBytes);

    /// Return all names of the enabled AOVs
    [[nodiscard]] inline const std::vector<std::string>& aovs() const { return mTechniqueInfo.EnabledAOVs; }

    // A frame consists of multiple iterations until target SPP is (ever) reached.
    // An iteration consists of SPI samples per iteration.
    // See https://pearcoding.github.io/Ignis/src/getting_started/realtime.html for more information

    /// Return number of samples rendered so far
    [[nodiscard]] inline size_t currentSampleCount() const { return mCurrentSampleCount; }
    /// Return number of iterations rendered so far
    [[nodiscard]] inline size_t currentIterationCount() const { return mCurrentIteration; }
    /// Return number of frames rendered so far
    [[nodiscard]] inline size_t currentFrameCount() const { return mCurrentFrame; }
    /// Return timepoint when the rendering started
    [[nodiscard]] inline Timepoint renderStartTime() const { return mStartTime; }
    /// Return seed used for the random generator
    [[nodiscard]] inline size_t seed() const { return mOptions.Seed; }

    /// Increase frame count (only used in interactive/realtime sessions)
    inline void incFrameCount() { mCurrentFrame++; }

    /// Return pointer to structure containing statistics
    [[nodiscard]] const Statistics* statistics() const;

    /// Returns the name of the loaded technique
    [[nodiscard]] inline const std::string& technique() const { return mTechniqueName; }

    /// Returns the name of the loaded camera
    [[nodiscard]] inline const std::string& camera() const { return mCameraName; }

    /// Return true if the runtime is used in tracing mode
    [[nodiscard]] inline bool isTrace() const { return mOptions.IsTracer; }

    /// The target the runtime is using
    [[nodiscard]] inline const Target& target() const { return mOptions.Target; }

    /// Computes (approximative) number of samples per iteration. This might be off due to the internal computing of techniques
    [[nodiscard]] inline size_t samplesPerIteration() const
    {
        // Try some iterations to get a SPI. The first non-zero one will be used
        for (size_t iter = 0; iter < 42; ++iter) {
            const size_t spi = mTechniqueInfo.ComputeSPI(iter, mSamplesPerIteration);
            if (spi != 0)
                return spi;
        }

        return 1; // Fallback
    }

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
    /// Set string parameter in the registry. Will replace already present values
    void setParameter(const std::string& name, const std::string& value);

    /// Get read-only registry
    [[nodiscard]] inline const ParameterSet& parameters() const { return mGlobalRegistry; }
    /// Get modifiable registry. A reset might be needed when changing parameters!
    [[nodiscard]] inline ParameterSet& accessParameters() { return mGlobalRegistry; }
    /// Merge parameters from other registry
    void mergeParametersFrom(const ParameterSet& other);

    /// The current framebuffer width
    [[nodiscard]] inline size_t framebufferWidth() const { return mFilmWidth; }
    /// The current framebuffer height
    [[nodiscard]] inline size_t framebufferHeight() const { return mFilmHeight; }

    /// Save the current framebuffer as a multilayered exr file with metadata attached
    bool saveFramebuffer(const Path& path) const;

    /// The initial camera orientation the scene was loaded with. Can be used to reset in later iterations
    [[nodiscard]] inline CameraOrientation initialCameraOrientation() const { return mInitialCameraOrientation; }
    /// Set internal parameters for the camera orientation. This is only a convenient wrapper around multiple setParameter calls
    void setCameraOrientation(const CameraOrientation& orientation);
    /// Get camera orientation from internal parameters. This is only a convenient wrapper around multiple getParameter calls
    CameraOrientation getCameraOrientation() const;

    /// True if denoising can be applied
    [[nodiscard]] static bool hasDenoiser();

    /// True if the scene has entries in the `parameters` section
    [[nodiscard]] inline bool hasSceneParameters() const { return !sceneParameterDesc().empty(); }
    [[nodiscard]] inline const ParameterDescSet sceneParameterDesc() const { return mSceneParameterDesc; }

    /// Get a list of all available techniques
    [[nodiscard]] static std::vector<std::string> getAvailableTechniqueTypes();

    /// Get a list of all available cameras
    [[nodiscard]] static std::vector<std::string> getAvailableCameraTypes();

    /// Get options used while creating the runtime
    [[nodiscard]] inline const RuntimeOptions& options() const { return mOptions; }
    [[nodiscard]] LoaderOptions loaderOptions() const;

    [[nodiscard]] std::shared_ptr<RenderPass> createPass(const std::string& src);
    bool runPass(const RenderPass& pass);

private:
    void checkCacheDirectory();
    bool load(const Path& path, const Scene* scene);
    bool setupScene();
    bool compileShaders();
    void stepVariant(size_t variant);
    void traceVariant(const std::vector<Ray>& rays, size_t variant);
    void handleTime();

    const RuntimeOptions mOptions;

    SceneDatabase mDatabase;
    ParameterSet mGlobalRegistry;
    ParameterDescSet mSceneParameterDesc; // Optional user parameter description given for a scene

    std::unique_ptr<ScriptCompiler> mCompiler;
    std::unique_ptr<IRenderDevice> mDevice;

    std::unique_ptr<OIDN> mDenoiser;

    size_t mSamplesPerIteration;

    size_t mCurrentIteration;
    size_t mCurrentSampleCount;
    size_t mCurrentFrame;

    Timepoint mStartTime;

    size_t mFilmWidth;
    size_t mFilmHeight;

    std::string mCameraName;
    CameraOrientation mInitialCameraOrientation;

    std::string mTechniqueName;
    TechniqueInfo mTechniqueInfo;

    std::vector<std::string> mResourceMap;
    std::vector<int> mEntityPerMaterial;

    std::vector<TechniqueVariant> mTechniqueVariants;
    std::vector<TechniqueVariantShaderSet> mTechniqueVariantShaderSets; // Compiled shaders
};
} // namespace IG