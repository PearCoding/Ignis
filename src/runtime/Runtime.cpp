#include "Runtime.h"
#include "Logger.h"
#include "RuntimeInfo.h"
#include "loader/Parser.h"

#include <chrono>
#include <fstream>

namespace IG {

static inline void setup_technique(LoaderOptions& lopts, const RuntimeOptions& opts)
{
    std::string tech_type;
    if (opts.OverrideTechnique.empty()) {
        const auto technique = lopts.Scene.technique();
        if (technique)
            tech_type = technique->pluginType();
        else
            tech_type = "path";
    } else {
        tech_type = opts.OverrideTechnique;
    }
    lopts.TechniqueType = tech_type;
}

static inline void setup_film(LoaderOptions& lopts, const RuntimeOptions& opts)
{
    Vector2f filmSize = Vector2f(800, 600);
    const auto film   = lopts.Scene.film();
    if (film)
        filmSize = film->property("size").getVector2(filmSize);

    lopts.FilmWidth  = opts.OverrideFilmSize.first > 0 ? (int)opts.OverrideFilmSize.first : (int)filmSize.x();
    lopts.FilmHeight = opts.OverrideFilmSize.second > 0 ? (int)opts.OverrideFilmSize.second : (int)filmSize.y();

    lopts.FilmWidth  = std::max<size_t>(1, lopts.FilmWidth);
    lopts.FilmHeight = std::max<size_t>(1, lopts.FilmHeight);

    lopts.PixelSamplerType = "independent";
    if (film)
        lopts.PixelSamplerType = to_lowercase(film->property("sampler").getString(lopts.PixelSamplerType));
}

static inline void setup_camera(LoaderOptions& lopts, const RuntimeOptions& opts)
{
    // Extract camera type
    std::string camera_type = "perspective";
    if (!opts.OverrideCamera.empty()) {
        camera_type = opts.OverrideCamera;
    } else {
        const auto camera = lopts.Scene.camera();
        if (camera)
            camera_type = camera->pluginType();
    }

    lopts.CameraType = camera_type;
}

static inline size_t recommendSPI(const Target& target, size_t width, size_t height, bool interactive)
{
    // The "best" case was measured with a 1000 x 1000. It does depend on the scene content though, but thats ignored here
    size_t spi_f = target.isCPU() ? 2 : 8;
    if (interactive)
        spi_f /= 2;
    const size_t spi = (size_t)std::ceil(spi_f / ((width / 1000.0f) * (height / 1000.0f)));
    return std::max<size_t>(1, std::min<size_t>(64, spi));
}

static inline void dumpShader(const std::string& filename, const std::string& shader)
{
    std::ofstream stream(filename);
    stream << shader;
}

Runtime::Runtime(const RuntimeOptions& opts)
    : mOptions(opts)
    , mDatabase()
    , mParameterSet()
    , mSamplesPerIteration(0)
    , mTarget(opts.Target)
    , mCurrentIteration(0)
    , mCurrentSampleCount(0)
    , mCurrentFrame(0)
    , mFilmWidth(0)
    , mFilmHeight(0)
    , mCameraName()
    , mInitialCameraOrientation()
    , mAcquireStats(opts.AcquireStats)
    , mTechniqueName()
    , mTechniqueInfo()
    , mTechniqueVariants()
    , mTechniqueVariantShaderSets()
{
    checkCacheDirectory();

    // Configure compiler
    mCompiler.setOptimizationLevel(std::min<size_t>(3, mOptions.ShaderOptimizationLevel));
    mCompiler.setVerbose(IG_LOGGER.verbosity() == L_DEBUG);

    // Check configuration
    if (!mTarget.isValid())
        throw std::runtime_error("Could not pick a suitable target");

    // Load interface
    IG_LOG(L_INFO) << "Using target " << mTarget.toString() << std::endl;

    // Load standard library if necessary
    if (!mOptions.ScriptDir.empty()) {
        IG_LOG(L_INFO) << "Loading standard library from " << mOptions.ScriptDir << std::endl;
        mCompiler.loadStdLibFromDirectory(mOptions.ScriptDir);
    }
}

Runtime::~Runtime()
{
}

void Runtime::checkCacheDirectory()
{
    constexpr size_t WarnSize = 1024 * 1024 * 1024 * 10ULL; // 10GB
    const size_t size         = RuntimeInfo::cacheDirectorySize();
    const auto dir            = RuntimeInfo::cacheDirectory();

    if (dir.empty())
        return;

    if (size >= WarnSize)
        IG_LOG(L_WARNING) << "Cache directory " << dir << " occupies " << FormatMemory(size) << " of disk space and exceeds " << FormatMemory(WarnSize) << " warn limit " << std::endl;
    else
        IG_LOG(L_DEBUG) << "Cache directory " << dir << " occupies " << FormatMemory(size) << " of disk space" << std::endl;
}

bool Runtime::loadFromFile(const std::filesystem::path& path)
{
    // Parse scene file
    IG_LOG(L_DEBUG) << "Parsing scene file" << std::endl;
    try {
        const auto startParser = std::chrono::high_resolution_clock::now();
        Parser::SceneParser parser;
        bool ok    = false;
        auto scene = parser.loadFromFile(path, ok);
        IG_LOG(L_DEBUG) << "Parsing scene took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startParser).count() / 1000.0f << " seconds" << std::endl;
        if (!ok)
            return false;

        if (mOptions.AddExtraEnvLight)
            scene.addConstantEnvLight();

        return load(path, std::move(scene));
    } catch (const std::runtime_error& err) {
        IG_LOG(L_ERROR) << "Loading error: " << err.what() << std::endl;
        return false;
    }
}

bool Runtime::loadFromString(const std::string& str)
{
    // Parse scene string
    try {
        IG_LOG(L_DEBUG) << "Parsing scene string" << std::endl;
        const auto startParser = std::chrono::high_resolution_clock::now();
        Parser::SceneParser parser;
        bool ok    = false;
        auto scene = parser.loadFromString(str, ok);
        IG_LOG(L_DEBUG) << "Parsing scene took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startParser).count() / 1000.0f << " seconds" << std::endl;
        if (!ok)
            return false;

        if (mOptions.AddExtraEnvLight)
            scene.addConstantEnvLight();

        return load({}, std::move(scene));
    } catch (const std::runtime_error& err) {
        IG_LOG(L_ERROR) << "Loading error: " << err.what() << std::endl;
        return false;
    }
}

bool Runtime::load(const std::filesystem::path& path, Parser::Scene&& scene)
{
    LoaderOptions lopts;
    lopts.FilePath            = path;
    lopts.Target              = mTarget;
    lopts.IsTracer            = mOptions.IsTracer;
    lopts.Scene               = std::move(scene);
    lopts.ForceSpecialization = mOptions.ForceSpecialization;
    lopts.EnableTonemapping   = mOptions.EnableTonemapping;
    lopts.Denoiser            = mOptions.Denoiser;
    lopts.Denoiser.Enabled    = !mOptions.IsTracer && mOptions.Denoiser.Enabled && hasDenoiser();

    // Print a warning if denoiser was requested but none is available
    if (mOptions.Denoiser.Enabled && !lopts.Denoiser.Enabled && !mOptions.IsTracer && hasDenoiser())
        IG_LOG(L_WARNING) << "Trying to use denoiser but no denoiser is available" << std::endl;

    if (lopts.Denoiser.Enabled)
        IG_LOG(L_INFO) << "Using denoiser" << std::endl;

    // Extract technique
    setup_technique(lopts, mOptions);

    // Extract film
    setup_film(lopts, mOptions);
    mFilmWidth  = lopts.FilmWidth;
    mFilmHeight = lopts.FilmHeight;

    // Extract camera
    setup_camera(lopts, mOptions);

    if (mOptions.SPI == 0)
        mSamplesPerIteration = recommendSPI(mTarget, mFilmWidth, mFilmHeight, mOptions.IsInteractive);
    else
        mSamplesPerIteration = mOptions.SPI;

    lopts.SamplesPerIteration = mSamplesPerIteration;
    IG_LOG(L_DEBUG) << "Recommended samples per iteration = " << mSamplesPerIteration << std::endl;

    IG_LOG(L_DEBUG) << "Loading scene" << std::endl;
    const auto startLoader = std::chrono::high_resolution_clock::now();
    LoaderResult result;
    if (!Loader::load(lopts, result))
        return false;
    mDatabase = std::move(result.Database);
    IG_LOG(L_DEBUG) << "Loading scene took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startLoader).count() / 1000.0f << " seconds" << std::endl;

    mCameraName               = lopts.CameraType;
    mTechniqueName            = lopts.TechniqueType;
    mTechniqueInfo            = result.TechniqueInfo;
    mInitialCameraOrientation = result.CameraOrientation;
    mTechniqueVariants        = std::move(result.TechniqueVariants);
    mResourceMap              = std::move(result.ResourceMap);

    if (lopts.Denoiser.Enabled)
        mTechniqueInfo.EnabledAOVs.emplace_back("Denoised");

    // Preload camera orientation
    setCameraOrientationParameter(mInitialCameraOrientation);
    return setup();
}

void Runtime::step(bool ignoreDenoiser)
{
    if (mOptions.IsTracer) {
        IG_LOG(L_ERROR) << "Trying to use step() in a trace driver!" << std::endl;
        return;
    }

    if (mTechniqueVariants.empty()) {
        IG_LOG(L_ERROR) << "No scene loaded!" << std::endl;
        return;
    }

    if (!mDevice) {
        IG_LOG(L_ERROR) << "Device not setup!" << std::endl;
        return;
    }

    if (mTechniqueInfo.VariantSelector) {
        const auto active = mTechniqueInfo.VariantSelector(mCurrentIteration);

        IG_ASSERT(active.size() > 0, "Expected some variants to be returned by the technique variant selector");

        for (size_t i = 0; i < active.size(); ++i)
            stepVariant(ignoreDenoiser, active[i], i == active.size() - 1);
    } else {
        for (size_t i = 0; i < mTechniqueVariants.size(); ++i)
            stepVariant(ignoreDenoiser, (int)i, i == mTechniqueVariants.size() - 1);
    }

    ++mCurrentIteration;
}

void Runtime::stepVariant(bool ignoreDenoiser, size_t variant, bool lastVariant)
{
    IG_ASSERT(variant < mTechniqueVariants.size(), "Expected technique variant to be well selected");
    const auto& info = mTechniqueInfo.Variants[variant];

    // IG_LOG(L_DEBUG) << "Rendering iteration " << mCurrentIteration << ", variant " << variant << std::endl;

    // Only apply denoiser after the final pass
    if (!lastVariant)
        ignoreDenoiser = true;

    Device::RenderSettings settings;
    settings.rays           = nullptr; // No artificial ray streams
    settings.apply_denoiser = mOptions.Denoiser.Enabled && !ignoreDenoiser;
    settings.spi            = info.GetSPI(mSamplesPerIteration);
    settings.work_width     = info.GetWidth(mFilmWidth);
    settings.work_height    = info.GetHeight(mFilmHeight);
    settings.info           = info;
    settings.iteration      = mCurrentIteration;
    settings.frame          = mCurrentFrame;

    setParameter("__spi", (int)settings.spi);
    mDevice->render(mTechniqueVariantShaderSets.at(variant), settings, &mParameterSet);

    if (!info.LockFramebuffer)
        mCurrentSampleCount += settings.spi;
}

void Runtime::trace(const std::vector<Ray>& rays)
{
    if (!mOptions.IsTracer) {
        IG_LOG(L_ERROR) << "Trying to use trace() in a camera driver!" << std::endl;
        return;
    }

    if (mTechniqueVariants.empty()) {
        IG_LOG(L_ERROR) << "No scene loaded!" << std::endl;
        return;
    }

    if (!mDevice) {
        IG_LOG(L_ERROR) << "Device not setup!" << std::endl;
        return;
    }

    if (mTechniqueInfo.VariantSelector) {
        const auto& active = mTechniqueInfo.VariantSelector(mCurrentIteration);
        for (const auto& ind : active)
            traceVariant(rays, ind);
    } else {
        for (size_t i = 0; i < mTechniqueVariants.size(); ++i)
            traceVariant(rays, i);
    }

    ++mCurrentIteration;
}

void Runtime::trace(const std::vector<Ray>& rays, std::vector<float>& data)
{
    trace(rays);

    // Get result
    const float* data_ptr = getFramebuffer({}).Data;
    data.resize(rays.size() * 3);
    std::memcpy(data.data(), data_ptr, sizeof(float) * data.size());
}

void Runtime::traceVariant(const std::vector<Ray>& rays, size_t variant)
{
    IG_ASSERT(variant < mTechniqueVariants.size(), "Expected technique variant to be well selected");
    const auto& info = mTechniqueInfo.Variants[variant];

    // IG_LOG(L_DEBUG) << "Tracing iteration " << mCurrentIteration << ", variant " << variant << std::endl;

    Device::RenderSettings settings;
    settings.rays           = rays.data();
    settings.apply_denoiser = false;
    settings.spi            = info.GetSPI(mSamplesPerIteration);
    settings.work_width     = rays.size();
    settings.work_height    = 1;
    settings.info           = info;
    settings.iteration      = mCurrentIteration;
    settings.frame          = mCurrentFrame;

    setParameter("__spi", (int)settings.spi);
    mDevice->render(mTechniqueVariantShaderSets.at(variant), settings, &mParameterSet);

    if (!info.LockFramebuffer)
        mCurrentSampleCount += settings.spi;
}

void Runtime::resizeFramebuffer(size_t width, size_t height)
{
    IG_ASSERT(mDevice, "Expected device to be available");
    mFilmWidth  = width;
    mFilmHeight = height;
    if (mDevice)
        mDevice->resize(width, height);
    reset();
}

AOVAccessor Runtime::getFramebuffer(const std::string& name) const
{
    IG_ASSERT(mDevice, "Expected device to be available");
    if (mDevice)
        return mDevice->getFramebuffer(name);
    else
        return AOVAccessor{ nullptr, 0 };
}

void Runtime::clearFramebuffer()
{
    IG_ASSERT(mDevice, "Expected device to be available");
    if (mDevice)
        mDevice->clearAllFramebuffer();
}

void Runtime::clearFramebuffer(const std::string& name)
{
    IG_ASSERT(mDevice, "Expected device to be available");
    if (mDevice)
        mDevice->clearFramebuffer(name);
}

void Runtime::reset()
{
    clearFramebuffer();
    mCurrentIteration   = 0;
    mCurrentSampleCount = 0;
    // No mCurrentFrameCount
}

const Statistics* Runtime::getStatistics() const
{
    IG_ASSERT(mDevice, "Expected device to be available");
    return mAcquireStats && mDevice ? mDevice->getStatistics() : nullptr;
}

bool Runtime::setup()
{
    Device::SetupSettings settings;
    settings.target             = mTarget;
    settings.database           = &mDatabase;
    settings.framebuffer_width  = (uint32)mFilmWidth;
    settings.framebuffer_height = (uint32)mFilmHeight;
    settings.acquire_stats      = mAcquireStats;
    settings.aov_map            = &mTechniqueInfo.EnabledAOVs;
    settings.resource_map       = &mResourceMap;

    IG_LOG(L_DEBUG) << "Init device" << std::endl;
    mDevice = std::make_unique<Device>(settings);

    if (IG_LOGGER.verbosity() <= L_DEBUG) {
        if (mResourceMap.empty()) {
            IG_LOG(L_DEBUG) << "Registered resources: None" << std::endl;
        } else {
            IG_LOG(L_DEBUG) << "Registered resources:" << std::endl;
            for (const auto& resource : mResourceMap)
                IG_LOG(L_DEBUG) << " -> " << resource << std::endl;
        }

        if (mDatabase.DynTables.empty()) {
            IG_LOG(L_DEBUG) << "Registered dyntables: None" << std::endl;
        } else {
            IG_LOG(L_DEBUG) << "Registered dyntables:" << std::endl;
            for (const auto& resource : mDatabase.DynTables)
                IG_LOG(L_DEBUG) << " -> " << resource.first << std::endl;
        }

        if (mDatabase.FixTables.empty()) {
            IG_LOG(L_DEBUG) << "Registered buffers: None" << std::endl;
        } else {
            IG_LOG(L_DEBUG) << "Registered buffers:" << std::endl;
            for (const auto& resource : mDatabase.FixTables)
                IG_LOG(L_DEBUG) << " -> " << resource.first << std::endl;
        }
    }

    if (!compileShaders())
        return false;

    clearFramebuffer();
    return true;
}

void Runtime::shutdown()
{
    mDevice.reset();
}

bool Runtime::compileShaders()
{
    const auto compile = [this](size_t i, const std::string& name, const std::string& func, const ShaderOutput<std::string>& input, ShaderOutput<void*>& output) {
        IG_LOG(L_DEBUG) << "Compiling " << name << " shader" << std::endl;
        output.Exec          = compileShader(input.Exec, func, "v" + std::to_string(i) + "_" + whitespace_escaped(name));
        output.LocalRegistry = input.LocalRegistry;
        if (output.Exec == nullptr)
            throw std::runtime_error("Failed to compile " + name + " shader in variant " + std::to_string(i) + ".");
    };

    const auto startJIT = std::chrono::high_resolution_clock::now();

    mTechniqueVariantShaderSets.resize(mTechniqueVariants.size());
    try {
        for (size_t i = 0; i < mTechniqueVariants.size(); ++i) {
            const auto& variant = mTechniqueVariants[i];
            auto& shaders       = mTechniqueVariantShaderSets[i];
            shaders.ID          = (uint32)i;

            IG_LOG(L_DEBUG) << "Handling technique variant " << i << std::endl;
            compile(i, "device", "ig_callback_shader", variant.DeviceShader, shaders.DeviceShader);
            if (mOptions.EnableTonemapping) {
                compile(i, "tonemap", "ig_tonemap_shader", variant.TonemapShader, shaders.TonemapShader);
                compile(i, "imageinfo", "ig_imageinfo_shader", variant.ImageinfoShader, shaders.ImageinfoShader);
            }
            compile(i, "primary traversal", "ig_traversal_shader", variant.PrimaryTraversalShader, shaders.PrimaryTraversalShader);
            compile(i, "secondary traversal", "ig_traversal_shader", variant.SecondaryTraversalShader, shaders.SecondaryTraversalShader);
            compile(i, "ray generation", "ig_ray_generation_shader", variant.RayGenerationShader, shaders.RayGenerationShader);
            compile(i, "miss", "ig_miss_shader", variant.MissShader, shaders.MissShader);

            IG_LOG(L_DEBUG) << "Compiling hit shaders" << std::endl;
            for (size_t j = 0; j < variant.HitShaders.size(); ++j) {
                ShaderOutput<void*> result;
                compile(i, "hit shader " + std::to_string(j), "ig_hit_shader", variant.HitShaders.at(j), result);
                shaders.HitShaders.push_back(result);
            }

            if (!variant.AdvancedShadowHitShaders.empty()) {
                IG_LOG(L_DEBUG) << "Compiling advanced shadow shaders" << std::endl;

                for (size_t j = 0; j < variant.AdvancedShadowHitShaders.size(); ++j) {
                    ShaderOutput<void*> result;
                    compile(i, "advanced shadow hit shader " + std::to_string(j), "ig_advanced_shadow_shader", variant.AdvancedShadowHitShaders.at(j), result);
                    shaders.AdvancedShadowHitShaders.push_back(result);
                }

                for (size_t j = 0; j < variant.AdvancedShadowMissShaders.size(); ++j) {
                    ShaderOutput<void*> result;
                    compile(i, "advanced shadow miss shader " + std::to_string(j), "ig_advanced_shadow_shader", variant.AdvancedShadowMissShaders.at(j), result);
                    shaders.AdvancedShadowMissShaders.push_back(result);
                }
            }

            for (size_t j = 0; j < variant.CallbackShaders.size(); ++j) {
                if (variant.CallbackShaders.at(j).Exec.empty()) {
                    shaders.CallbackShaders[j].Exec = nullptr;
                } else {
                    compile(i, "callback " + std::to_string(j), "ig_callback_shader", variant.CallbackShaders.at(j), shaders.CallbackShaders[j]);
                }
            }
        }
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        return false;
    }

    IG_LOG(L_DEBUG) << "Compiling shaders took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startJIT).count() / 1000.0f << " seconds" << std::endl;

    return true;
}

void* Runtime::compileShader(const std::string& src, const std::string& func, const std::string& name)
{
    if (mOptions.DumpShader)
        dumpShader(name + ".art", src);

    const std::string full_shader = mCompiler.prepare(src);

    if (mOptions.DumpShaderFull)
        dumpShader(name + "_full.art", full_shader);

    return mCompiler.compile(full_shader, func);
}

void Runtime::tonemap(uint32* out_pixels, const TonemapSettings& settings)
{
    if (mTechniqueVariants.empty()) {
        IG_LOG(L_ERROR) << "No scene loaded!" << std::endl;
        return;
    }

    IG_ASSERT(mDevice, "Expected device to be available");
    if (mDevice)
        mDevice->tonemap(out_pixels, settings);
}

ImageInfoOutput Runtime::imageinfo(const ImageInfoSettings& settings)
{
    if (mTechniqueVariants.empty()) {
        IG_LOG(L_ERROR) << "No scene loaded!" << std::endl;
        return ImageInfoOutput{};
    }

    IG_ASSERT(mDevice, "Expected device to be available");
    if (mDevice)
        return mDevice->imageinfo(settings);
    else
        return ImageInfoOutput{};
}

void Runtime::setParameter(const std::string& name, int value)
{
    mParameterSet.IntParameters[name] = value;
}

void Runtime::setParameter(const std::string& name, float value)
{
    mParameterSet.FloatParameters[name] = value;
}

void Runtime::setParameter(const std::string& name, const Vector3f& value)
{
    mParameterSet.VectorParameters[name] = value;
}

void Runtime::setParameter(const std::string& name, const Vector4f& value)
{
    mParameterSet.ColorParameters[name] = value;
}

std::vector<std::string> Runtime::getAvailableTechniqueTypes()
{
    return Loader::getAvailableTechniqueTypes();
}

std::vector<std::string> Runtime::getAvailableCameraTypes()
{
    return Loader::getAvailableCameraTypes();
}

void Runtime::setCameraOrientationParameter(const CameraOrientation& orientation)
{
    setParameter("__camera_eye", orientation.Eye);
    setParameter("__camera_dir", orientation.Dir);
    setParameter("__camera_up", orientation.Up);
}

bool Runtime::hasDenoiser() const
{
#ifdef IG_HAS_DENOISER
    return true;
#else
    return false;
#endif
}
} // namespace IG