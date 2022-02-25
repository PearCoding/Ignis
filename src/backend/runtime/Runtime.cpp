#include "Runtime.h"
#include "Logger.h"
#include "jit.h"
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

static inline size_t recommendSPI(Target target)
{
    if (isCPU(target))
        return 2;
    else
        return 8;
}

static inline void dumpShader(const std::string& filename, const std::string& shader)
{
    std::ofstream stream(filename);
    stream << shader;
}

static inline void dumpShader(const std::vector<TechniqueVariant>& variants)
{
    for (size_t i = 0; i < variants.size(); ++i) {
        const auto& variant = variants[i];
        dumpShader("v" + std::to_string(i) + "_rayGeneration.art", variant.RayGenerationShader);
        dumpShader("v" + std::to_string(i) + "_missShader.art", variant.MissShader);

        int counter = 0;
        for (const auto& shader : variant.HitShaders) {
            dumpShader("v" + std::to_string(i) + "_hitShader" + std::to_string(counter++) + ".art", shader);
        }

        if (!variant.AdvancedShadowHitShader.empty()) {
            dumpShader("v" + std::to_string(i) + "_advancedShadowHit.art", variant.AdvancedShadowHitShader);
            dumpShader("v" + std::to_string(i) + "_advancedShadowMiss.art", variant.AdvancedShadowMissShader);
        }

        for (size_t i = 0; i < variant.CallbackShaders.size(); ++i) {
            if (!variant.CallbackShaders[i].empty())
                dumpShader("v" + std::to_string(i) + "_callback" + std::to_string(i) + ".art", variant.CallbackShaders[i]);
        }
    }
}

Runtime::Runtime(const RuntimeOptions& opts)
    : mOptions(opts)
    , mDevice(opts.Device)
    , mCurrentIteration(0)
    , mCurrentSampleCount(0)
    , mIsTrace(opts.IsTracer)
    , mIsDebug(false)
    , mAcquireStats(opts.AcquireStats)
{
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
                          << targetToString(target) << " to "
                          << targetToString(newTarget) << std::endl;
    }
    mTarget = newTarget;

    // Load interface
    IG_LOG(L_INFO) << "Loading target " << targetToString(mTarget) << std::endl;
    if (!mManager.load(mTarget, mLoadedInterface))
        throw std::runtime_error("Error loading interface!");

        // Force flush to zero mode for denormals
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
    _mm_setcsr(_mm_getcsr() | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
#endif
}

Runtime::~Runtime()
{
    if (!mTechniqueVariants.empty())
        shutdown();
}

bool Runtime::loadFromFile(const std::filesystem::path& path)
{
    // Parse scene file
    IG_LOG(L_DEBUG) << "Parsing scene file" << std::endl;
    const auto startParser = std::chrono::high_resolution_clock::now();
    Parser::SceneParser parser;
    bool ok    = false;
    auto scene = parser.loadFromFile(path, ok);
    IG_LOG(L_DEBUG) << "Parsing scene took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startParser).count() / 1000.0f << " seconds" << std::endl;
    if (!ok)
        return false;

    return load(path, std::move(scene));
}

bool Runtime::loadFromString(const std::string& str)
{
    // Parse scene string
    IG_LOG(L_DEBUG) << "Parsing scene string" << std::endl;
    const auto startParser = std::chrono::high_resolution_clock::now();
    Parser::SceneParser parser;
    bool ok    = false;
    auto scene = parser.loadFromString(str, ok);
    IG_LOG(L_DEBUG) << "Parsing scene took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startParser).count() / 1000.0f << " seconds" << std::endl;
    if (!ok)
        return false;

    return load({}, std::move(scene));
}

bool Runtime::load(const std::filesystem::path& path, Parser::Scene&& scene)
{
    LoaderOptions lopts;
    lopts.FilePath = path;
    lopts.Target   = mTarget;
    lopts.IsTracer = mIsTrace;
    lopts.Scene    = std::move(scene);

    // Extract technique
    setup_technique(lopts, mOptions);

    // Extract film
    setup_film(lopts, mOptions);
    mFilmWidth  = lopts.FilmWidth;
    mFilmHeight = lopts.FilmHeight;

    // Extract camera
    setup_camera(lopts, mOptions);

    if (mOptions.SPI == 0)
        mSamplesPerIteration = recommendSPI(mTarget);
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

    mIsDebug                  = lopts.TechniqueType == "debug";
    mTechniqueInfo            = result.TechniqueInfo;
    mInitialCameraOrientation = result.CameraOrientation;

    mTechniqueVariants = std::move(result.TechniqueVariants);

    if (mOptions.DumpShader)
        dumpShader(mTechniqueVariants);

    return setup();
}

void Runtime::step()
{
    if (IG_UNLIKELY(mIsTrace)) {
        IG_LOG(L_ERROR) << "Trying to use step() in a trace driver!" << std::endl;
        return;
    }

    if (mTechniqueVariants.empty()) {
        IG_LOG(L_ERROR) << "No scene loaded!" << std::endl;
        return;
    }

    if (mTechniqueInfo.VariantSelector) {
        const auto& active = mTechniqueInfo.VariantSelector(mCurrentIteration);
        for (const auto& ind : active)
            stepVariant(ind);
    } else {
        for (size_t i = 0; i < mTechniqueVariants.size(); ++i)
            stepVariant((int)i);
    }

    ++mCurrentIteration;
}

void Runtime::stepVariant(int variant)
{
    IG_ASSERT(variant < (int)mTechniqueVariants.size(), "Expected technique variant to be well selected");
    const auto& info = mTechniqueInfo.Variants[variant];

    // IG_LOG(L_DEBUG) << "Rendering iteration " << mCurrentIteration << ", variant " << variant << std::endl;

    DriverRenderSettings settings;
    settings.rays               = nullptr; // No artifical ray streams
    settings.device             = mDevice;
    settings.spi                = info.GetSPI(mSamplesPerIteration);
    settings.work_width         = info.GetWidth(mFilmWidth);
    settings.work_height        = info.GetHeight(mFilmHeight);
    settings.framebuffer_locked = info.LockFramebuffer;

    mLoadedInterface.RenderFunction(mTechniqueVariantShaderSets[variant], settings, &mParameterSet, mCurrentIteration);

    if (!info.LockFramebuffer)
        mCurrentSampleCount += settings.spi;
}

void Runtime::trace(const std::vector<Ray>& rays, std::vector<float>& data)
{
    if (IG_UNLIKELY(!mIsTrace)) {
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
            traceVariant(rays, (int)i);
    }

    ++mCurrentIteration;

    // Get result
    const float* data_ptr = getFramebuffer(0);
    data.resize(rays.size() * 3);
    std::memcpy(data.data(), data_ptr, sizeof(float) * rays.size() * 3);
}

void Runtime::traceVariant(const std::vector<Ray>& rays, int variant)
{
    IG_ASSERT(variant < (int)mTechniqueVariants.size(), "Expected technique variant to be well selected");
    const auto& info = mTechniqueInfo.Variants[variant];

    // IG_LOG(L_DEBUG) << "Tracing iteration " << mCurrentIteration << ", variant " << variant << std::endl;

    DriverRenderSettings settings;
    settings.rays               = rays.data();
    settings.device             = mDevice;
    settings.spi                = info.GetSPI(mSamplesPerIteration);
    settings.work_width         = rays.size();
    settings.work_height        = 1;
    settings.framebuffer_locked = info.LockFramebuffer;

    mLoadedInterface.RenderFunction(mTechniqueVariantShaderSets[variant], settings, &mParameterSet, mCurrentIteration);

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

const float* Runtime::getFramebuffer(int aov) const
{
    return mLoadedInterface.GetFramebufferFunction(aov);
}

void Runtime::clearFramebuffer(int aov)
{
    return mLoadedInterface.ClearFramebufferFunction(aov);
}

void Runtime::reset()
{
    clearFramebuffer();
    mCurrentIteration   = 0;
    mCurrentSampleCount = 0;
}

const Statistics* Runtime::getStatistics() const
{
    return mAcquireStats ? mLoadedInterface.GetStatisticsFunction() : nullptr;
}

bool Runtime::setup()
{
    DriverSetupSettings settings;
    settings.database           = &mDatabase;
    settings.framebuffer_width  = mFilmWidth;
    settings.framebuffer_height = mFilmHeight;
    settings.acquire_stats      = mAcquireStats;
    settings.aov_count          = mTechniqueInfo.EnabledAOVs.size();

    settings.logger = &IG_LOGGER;

    IG_LOG(L_DEBUG) << "Init JIT compiling" << std::endl;
    ig_init_jit(mManager.getPath(mTarget).generic_u8string());
    mLoadedInterface.SetupFunction(settings);

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
        IG_LOG(L_DEBUG) << "Compiling ray generation shader" << std::endl;
        const std::filesystem::path rgp = "v" + std::to_string(i) + "_rayGenerationFull.art";
        shaders.RayGenerationShader     = ig_compile_source(variant.RayGenerationShader, "ig_ray_generation_shader",
                                                        mOptions.DumpShaderFull ? &rgp : nullptr);
        if (shaders.RayGenerationShader == nullptr) {
            IG_LOG(L_ERROR) << "Failed to compile ray generation shader in variant " << i << "." << std::endl;
            return false;
        }

        IG_LOG(L_DEBUG) << "Compiling miss shader" << std::endl;
        const std::filesystem::path mp = "v" + std::to_string(i) + "_missShaderFull.art";
        shaders.MissShader             = ig_compile_source(variant.MissShader, "ig_miss_shader",
                                               mOptions.DumpShaderFull ? &mp : nullptr);
        if (shaders.MissShader == nullptr) {
            IG_LOG(L_ERROR) << "Failed to compile miss shader in variant " << i << "." << std::endl;
            return false;
        }

        IG_LOG(L_DEBUG) << "Compiling hit shaders" << std::endl;
        for (size_t j = 0; j < variant.HitShaders.size(); ++j) {
            IG_LOG(L_DEBUG) << "Hit shader [" << j << "]" << std::endl;
            const std::filesystem::path hp = "v" + std::to_string(i) + "_hitShaderFull" + std::to_string(j) + ".art";
            shaders.HitShaders.push_back(ig_compile_source(variant.HitShaders[j], "ig_hit_shader", mOptions.DumpShaderFull ? &hp : nullptr));
            if (shaders.HitShaders[j] == nullptr) {
                IG_LOG(L_ERROR) << "Failed to compile hit shader " << j << " in variant " << i << "." << std::endl;
                return false;
            }
        }

        if (!variant.AdvancedShadowHitShader.empty()) {
            IG_LOG(L_DEBUG) << "Compiling advanced shadow shaders" << std::endl;
            const std::filesystem::path ash = "v" + std::to_string(i) + "_advancedShadowHitFull.art";
            shaders.AdvancedShadowHitShader = ig_compile_source(variant.AdvancedShadowHitShader, "ig_advanced_shadow_shader",
                                                                mOptions.DumpShaderFull ? &ash : nullptr);

            if (shaders.AdvancedShadowHitShader == nullptr) {
                IG_LOG(L_ERROR) << "Failed to compile advanced shadow hit shader in variant " << i << "." << std::endl;
                return false;
            }

            const std::filesystem::path asm_ = "v" + std::to_string(i) + "_advancedShadowMissFull.art";
            shaders.AdvancedShadowMissShader = ig_compile_source(variant.AdvancedShadowMissShader, "ig_advanced_shadow_shader",
                                                                 mOptions.DumpShaderFull ? &asm_ : nullptr);

            if (shaders.AdvancedShadowMissShader == nullptr) {
                IG_LOG(L_ERROR) << "Failed to compile advanced shadow miss shader in variant " << i << "." << std::endl;
                return false;
            }
        }

        for (size_t j = 0; j < variant.CallbackShaders.size(); ++j) {
            if (variant.CallbackShaders[j].empty()) {
                shaders.CallbackShaders[j] = nullptr;
            } else {
                IG_LOG(L_DEBUG) << "Compiling callback shader [" << i << "]" << std::endl;
                const std::filesystem::path asm_ = " v" + std::to_string(i) + "_callbackFull" + std::to_string(j) + ".art";
                shaders.CallbackShaders[j]       = ig_compile_source(variant.CallbackShaders[j], "ig_callback_shader",
                                                               mOptions.DumpShaderFull ? &asm_ : nullptr);
                if (shaders.CallbackShaders[j] == nullptr) {
                    IG_LOG(L_ERROR) << "Failed to compile callback " << j << " shader in variant " << i << "." << std::endl;
                    return false;
                }
            }
        }
    }
    IG_LOG(L_DEBUG) << "Compiling shaders took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startJIT).count() / 1000.0f << " seconds" << std::endl;

    return true;
}

void Runtime::tonemap(uint32* out_pixels, const TonemapSettings& settings)
{
    if (mTechniqueVariants.empty()) {
        IG_LOG(L_ERROR) << "No scene loaded!" << std::endl;
        return;
    }

    mLoadedInterface.TonemapFunction(mDevice, out_pixels, settings);
}

void Runtime::imageinfo(const ImageInfoSettings& settings, ImageInfoOutput& output)
{
    if (mTechniqueVariants.empty()) {
        IG_LOG(L_ERROR) << "No scene loaded!" << std::endl;
        return;
    }

    mLoadedInterface.ImageInfoFunction(mDevice, settings, output);
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

} // namespace IG