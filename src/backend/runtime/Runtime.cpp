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

static inline size_t recommendSPI(Target target, size_t width, size_t height, bool interactive)
{
    // The "best" case was measured with a 1000 x 1000. It does depend on the scene content though, but thats ignored here
    size_t spi_f = TargetInfo(target).isCPU() ? 2 : 8;
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
    , mLoadedInterface()
    , mManager()
    , mParameterSet()
    , mDevice(opts.Device)
    , mSamplesPerIteration(0)
    , mTarget(Target::INVALID)
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

    if (!mManager.init(opts.ModulePath))
        throw std::runtime_error("Could not init modules!");

    // Recommend a target based on the loaded drivers
    Target target = opts.DesiredTarget;
    if (target == Target::INVALID) {
        if (opts.RecommendCPU && !opts.RecommendGPU)
            target = mManager.recommendCPUTarget();
        else if (!opts.RecommendCPU && opts.RecommendGPU)
            target = mManager.recommendGPUTarget();
        else
            target = mManager.recommendTarget();
    }

    // Check configuration
    const Target newTarget = mManager.resolveTarget(target);
    if (newTarget != target) {
        IG_LOG(L_WARNING) << "Switched from "
                          << TargetInfo(target).toString() << " to "
                          << TargetInfo(newTarget).toString() << std::endl;
    }
    mTarget = newTarget;

    // Load interface
    IG_LOG(L_INFO) << "Loading target " << TargetInfo(mTarget).toString() << std::endl;
    if (!mManager.load(mTarget, mLoadedInterface))
        throw std::runtime_error("Error loading interface!");

    // Load standard library if necessary
    if (!mOptions.ScriptDir.empty()) {
        IG_LOG(L_INFO) << "Loading standard library from " << mOptions.ScriptDir << std::endl;
        mScriptPreprocessor.loadStdLibFromDirectory(mOptions.ScriptDir);
    }
}

Runtime::~Runtime()
{
    if (!mTechniqueVariants.empty())
        shutdown();
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

    DriverRenderSettings settings;
    settings.rays           = nullptr; // No artificial ray streams
    settings.device         = mDevice;
    settings.apply_denoiser = mOptions.Denoiser.Enabled && !ignoreDenoiser;
    settings.spi            = info.GetSPI(mSamplesPerIteration);
    settings.work_width     = info.GetWidth(mFilmWidth);
    settings.work_height    = info.GetHeight(mFilmHeight);
    settings.info           = info;

    setParameter("__spi", (int)settings.spi);
    mLoadedInterface.RenderFunction(mTechniqueVariantShaderSets[variant], settings, &mParameterSet, mCurrentIteration, mCurrentFrame);

    if (!info.LockFramebuffer)
        mCurrentSampleCount += settings.spi;
}

void Runtime::trace(const std::vector<Ray>& rays, std::vector<float>& data)
{
    if (!mOptions.IsTracer) {
        IG_LOG(L_ERROR) << "Trying to use trace() in a camera driver!" << std::endl;
        return;
    }

    if (mTechniqueVariants.empty()) {
        IG_LOG(L_ERROR) << "No scene loaded!" << std::endl;
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

    // Get result
    const float* data_ptr = getFramebuffer({}).Data;
    data.resize(rays.size() * 3);
    std::memcpy(data.data(), data_ptr, sizeof(float) * rays.size() * 3);
}

void Runtime::traceVariant(const std::vector<Ray>& rays, size_t variant)
{
    IG_ASSERT(variant < mTechniqueVariants.size(), "Expected technique variant to be well selected");
    const auto& info = mTechniqueInfo.Variants[variant];

    // IG_LOG(L_DEBUG) << "Tracing iteration " << mCurrentIteration << ", variant " << variant << std::endl;

    DriverRenderSettings settings;
    settings.rays           = rays.data();
    settings.device         = mDevice;
    settings.apply_denoiser = false;
    settings.spi            = info.GetSPI(mSamplesPerIteration);
    settings.work_width     = rays.size();
    settings.work_height    = 1;
    settings.info           = info;

    setParameter("__spi", (int)settings.spi);
    mLoadedInterface.RenderFunction(mTechniqueVariantShaderSets[variant], settings, &mParameterSet, mCurrentIteration, mCurrentFrame);

    if (!info.LockFramebuffer)
        mCurrentSampleCount += settings.spi;
}

void Runtime::resizeFramebuffer(size_t width, size_t height)
{
    mFilmWidth  = width;
    mFilmHeight = height;
    mLoadedInterface.ResizeFramebufferFunction(width, height);
    reset();
}

AOVAccessor Runtime::getFramebuffer(const std::string& name) const
{
    DriverAOVAccessor acc;
    mLoadedInterface.GetFramebufferFunction((int)mDevice, name.c_str(), acc);
    return AOVAccessor{ acc.Data, acc.IterationCount };
}

void Runtime::clearFramebuffer()
{
    return mLoadedInterface.ClearAllFramebufferFunction();
}

void Runtime::clearFramebuffer(const std::string& name)
{
    return mLoadedInterface.ClearFramebufferFunction(name.c_str());
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
    return mAcquireStats ? mLoadedInterface.GetStatisticsFunction() : nullptr;
}

bool Runtime::setup()
{
    const std::string driver_filename = mManager.getPath(mTarget).generic_u8string();

    DriverSetupSettings settings;
    settings.driver_filename    = driver_filename.c_str();
    settings.database           = &mDatabase;
    settings.framebuffer_width  = (uint32)mFilmWidth;
    settings.framebuffer_height = (uint32)mFilmHeight;
    settings.acquire_stats      = mAcquireStats;
    settings.aov_map            = &mTechniqueInfo.EnabledAOVs;
    settings.resource_map       = &mResourceMap;

    settings.logger = &IG_LOGGER;

    IG_LOG(L_DEBUG) << "Init driver" << std::endl;
    mLoadedInterface.SetupFunction(settings);

    if (IG_LOGGER.verbosity() <= L_DEBUG) {
        if (mResourceMap.empty()) {
            IG_LOG(L_DEBUG) << "Registered resources: None" << std::endl;
        } else {
            IG_LOG(L_DEBUG) << "Registered resources:" << std::endl;
            for (const auto& resource : mResourceMap)
                IG_LOG(L_DEBUG) << " -> " << resource << std::endl;
        }
    }

    if (!compileShaders())
        return false;

    clearFramebuffer();
    return true;
}

void Runtime::shutdown()
{
    mLoadedInterface.ShutdownFunction();
}

bool Runtime::compileShaders()
{
    const auto startJIT = std::chrono::high_resolution_clock::now();
    mTechniqueVariantShaderSets.resize(mTechniqueVariants.size());
    for (size_t i = 0; i < mTechniqueVariants.size(); ++i) {
        const auto& variant = mTechniqueVariants[i];
        auto& shaders       = mTechniqueVariantShaderSets[i];

        IG_LOG(L_DEBUG) << "Handling technique variant " << i << std::endl;
        IG_LOG(L_DEBUG) << "Compiling primary traversal shader" << std::endl;
        shaders.PrimaryTraversalShader.Exec          = compileShader(variant.PrimaryTraversalShader.Exec, "ig_primary_traversal_shader", "v" + std::to_string(i) + "_primaryTraversal");
        shaders.PrimaryTraversalShader.LocalRegistry = variant.PrimaryTraversalShader.LocalRegistry;
        if (shaders.PrimaryTraversalShader.Exec == nullptr) {
            IG_LOG(L_ERROR) << "Failed to compile primary traversal shader in variant " << i << "." << std::endl;
            return false;
        }

        IG_LOG(L_DEBUG) << "Compiling secondary traversal shader" << std::endl;
        shaders.SecondaryTraversalShader.Exec          = compileShader(variant.SecondaryTraversalShader.Exec, "ig_secondary_traversal_shader", "v" + std::to_string(i) + "_secondaryTraversal");
        shaders.SecondaryTraversalShader.LocalRegistry = variant.SecondaryTraversalShader.LocalRegistry;
        if (shaders.SecondaryTraversalShader.Exec == nullptr) {
            IG_LOG(L_ERROR) << "Failed to compile secondary traversal shader in variant " << i << "." << std::endl;
            return false;
        }

        IG_LOG(L_DEBUG) << "Compiling ray generation shader" << std::endl;
        shaders.RayGenerationShader.Exec          = compileShader(variant.RayGenerationShader.Exec, "ig_ray_generation_shader", "v" + std::to_string(i) + "_rayGeneration");
        shaders.RayGenerationShader.LocalRegistry = variant.RayGenerationShader.LocalRegistry;
        if (shaders.RayGenerationShader.Exec == nullptr) {
            IG_LOG(L_ERROR) << "Failed to compile ray generation shader in variant " << i << "." << std::endl;
            return false;
        }

        IG_LOG(L_DEBUG) << "Compiling miss shader" << std::endl;
        shaders.MissShader.Exec          = compileShader(variant.MissShader.Exec, "ig_miss_shader", "v" + std::to_string(i) + "_missShader");
        shaders.MissShader.LocalRegistry = variant.MissShader.LocalRegistry;
        if (shaders.MissShader.Exec == nullptr) {
            IG_LOG(L_ERROR) << "Failed to compile miss shader in variant " << i << "." << std::endl;
            return false;
        }

        IG_LOG(L_DEBUG) << "Compiling hit shaders" << std::endl;
        for (size_t j = 0; j < variant.HitShaders.size(); ++j) {
            IG_LOG(L_DEBUG) << "Compiling Hit shader [" << j << "]" << std::endl;
            ShaderOutput<void*> result = {
                compileShader(variant.HitShaders[j].Exec, "ig_hit_shader", "v" + std::to_string(i) + "_hitShader" + std::to_string(j)),
                variant.HitShaders[j].LocalRegistry
            };
            shaders.HitShaders.push_back(result);
            if (result.Exec == nullptr) {
                IG_LOG(L_ERROR) << "Failed to compile hit shader " << j << " in variant " << i << "." << std::endl;
                return false;
            }
        }

        if (!variant.AdvancedShadowHitShaders.empty()) {
            IG_LOG(L_DEBUG) << "Compiling advanced shadow shaders" << std::endl;

            for (size_t j = 0; j < variant.AdvancedShadowHitShaders.size(); ++j) {
                IG_LOG(L_DEBUG) << "Compiling Advanced Shadow Hit shader [" << j << "]" << std::endl;
                ShaderOutput<void*> result = {
                    compileShader(variant.AdvancedShadowHitShaders[j].Exec, "ig_advanced_shadow_shader", "v" + std::to_string(i) + "_advancedShadowHit" + std::to_string(j)),
                    variant.AdvancedShadowHitShaders[j].LocalRegistry
                };
                shaders.AdvancedShadowHitShaders.push_back(result);

                if (shaders.AdvancedShadowHitShaders[j].Exec == nullptr) {
                    IG_LOG(L_ERROR) << "Failed to compile advanced shadow hit shader " << j << " in variant " << i << "." << std::endl;
                    return false;
                }
            }

            for (size_t j = 0; j < variant.AdvancedShadowMissShaders.size(); ++j) {
                IG_LOG(L_DEBUG) << "Compiling Advanced Shadow Miss shader [" << j << "]" << std::endl;
                ShaderOutput<void*> result = {
                    compileShader(variant.AdvancedShadowMissShaders[j].Exec, "ig_advanced_shadow_shader", "v" + std::to_string(i) + "_advancedShadowMiss" + std::to_string(j)),
                    variant.AdvancedShadowMissShaders[j].LocalRegistry
                };
                shaders.AdvancedShadowMissShaders.push_back(result);
                if (shaders.AdvancedShadowMissShaders[j].Exec == nullptr) {
                    IG_LOG(L_ERROR) << "Failed to compile advanced shadow miss shader " << j << " in variant " << i << "." << std::endl;
                    return false;
                }
            }
        }

        for (size_t j = 0; j < variant.CallbackShaders.size(); ++j) {
            if (variant.CallbackShaders.at(j).Exec.empty()) {
                shaders.CallbackShaders[j].Exec = nullptr;
            } else {
                IG_LOG(L_DEBUG) << "Compiling callback shader [" << i << "]" << std::endl;
                shaders.CallbackShaders[j].Exec          = compileShader(variant.CallbackShaders.at(j).Exec, "ig_callback_shader", "v" + std::to_string(i) + "_callback" + std::to_string(j));
                shaders.CallbackShaders[j].LocalRegistry = variant.CallbackShaders.at(j).LocalRegistry;
                if (shaders.CallbackShaders.at(j).Exec == nullptr) {
                    IG_LOG(L_ERROR) << "Failed to compile callback " << j << " shader in variant " << i << "." << std::endl;
                    return false;
                }
            }
        }
    }
    IG_LOG(L_DEBUG) << "Compiling shaders took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startJIT).count() / 1000.0f << " seconds" << std::endl;

    return true;
}

void* Runtime::compileShader(const std::string& src, const std::string& func, const std::string& name)
{
    if (mOptions.DumpShader)
        dumpShader(name + ".art", src);

    const std::string full_shader = mScriptPreprocessor.prepare(src);

    if (mOptions.DumpShaderFull)
        dumpShader(name + "_full.art", full_shader);

    return mLoadedInterface.CompileSourceFunction(full_shader.c_str(), func.c_str(), IG_LOGGER.verbosity() == L_DEBUG);
}

void Runtime::tonemap(uint32* out_pixels, const TonemapSettings& settings)
{
    if (mTechniqueVariants.empty()) {
        IG_LOG(L_ERROR) << "No scene loaded!" << std::endl;
        return;
    }

    mLoadedInterface.TonemapFunction((int)mDevice, out_pixels, settings);
}

void Runtime::imageinfo(const ImageInfoSettings& settings, ImageInfoOutput& output)
{
    if (mTechniqueVariants.empty()) {
        IG_LOG(L_ERROR) << "No scene loaded!" << std::endl;
        return;
    }

    mLoadedInterface.ImageInfoFunction((int)mDevice, settings, output);
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
} // namespace IG