#include "Runtime.h"
#include "Camera.h"
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

static inline void setup_film(RuntimeRenderSettings& settings, const LoaderOptions& lopts, const RuntimeOptions& opts)
{
    Vector2f filmSize = Vector2f(settings.FilmWidth, settings.FilmHeight);
    const auto film   = lopts.Scene.film();
    if (film)
        filmSize = film->property("size").getVector2(filmSize);

    settings.FilmWidth  = opts.OverrideFilmSize.first > 0 ? opts.OverrideFilmSize.first : filmSize.x();
    settings.FilmHeight = opts.OverrideFilmSize.second > 0 ? opts.OverrideFilmSize.second : filmSize.y();
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

static inline void setup_camera_view(RuntimeRenderSettings& settings, const LoaderOptions& lopts, const BoundingBox& sceneBBox)
{
    if (!lopts.Scene.camera()) {
        // If no camera information is given whatsoever, try to setup a view over the whole scene
        const float aspect_ratio = settings.FilmWidth / settings.FilmHeight;
        const float a            = sceneBBox.diameter().x() / 2;
        const float b            = sceneBBox.diameter().y() / (2 * aspect_ratio);
        const float s            = std::sin(settings.FOV * Deg2Rad / 2);
        const float d            = std::max(a, b) * std::sqrt(1 / (s * s) - 1);

        settings.CameraDir = -Vector3f::UnitZ();
        settings.CameraUp  = Vector3f::UnitY();
        settings.CameraEye = Vector3f::UnitX() * sceneBBox.center().x()
                             + Vector3f::UnitY() * sceneBBox.center().y()
                             + Vector3f::UnitZ() * (sceneBBox.max.z() + d);
    } else {
        // Get initial location
        Transformf cameraTransform = Transformf::Identity();
        const auto camera          = lopts.Scene.camera();
        if (camera) {
            cameraTransform = camera->property("transform").getTransform();
            settings.FOV    = camera->property("fov").getNumber(settings.FOV);

            settings.TMin = camera->property("near_clip").getNumber(settings.TMin);
            settings.TMax = camera->property("far_clip").getNumber(settings.TMax);
        }

        settings.CameraEye = cameraTransform * Vector3f::Zero();
        settings.CameraDir = cameraTransform.linear().col(2);
        settings.CameraUp  = cameraTransform.linear().col(1);

        if (settings.TMax < settings.TMin)
            std::swap(settings.TMin, settings.TMax);
    }
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

Runtime::Runtime(const std::filesystem::path& path, const RuntimeOptions& opts)
    : mInit(false)
    , mOptions(opts)
    , mDevice(opts.Device)
    , mCurrentIteration(0)
    , mCurrentSampleCount(0)
    , mIsTrace(false)
    , mIsDebug(false)
    , mDebugMode(DebugMode::Normal)
    , mAcquireStats(opts.AcquireStats)
{
    if (!mManager.init())
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

    LoaderOptions lopts;
    lopts.FilePath = path;
    lopts.Target   = target;

    // Parse scene file
    IG_LOG(L_DEBUG) << "Parsing scene" << std::endl;
    const auto startParser = std::chrono::high_resolution_clock::now();
    Parser::SceneParser parser;
    bool ok     = false;
    lopts.Scene = parser.loadFromFile(path, ok);
    if (!ok)
        throw std::runtime_error("Could not parse scene!");
    IG_LOG(L_DEBUG) << "Parsing scene took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startParser).count() / 1000.0f << " seconds" << std::endl;

    // Extract technique
    setup_technique(lopts, opts);

    // Extract film
    setup_film(mLoadedRenderSettings, lopts, opts);

    // Extract camera
    setup_camera(lopts, opts);

    // Check configuration
    const Target newTarget = mManager.resolveTarget(lopts.Target);
    if (newTarget != lopts.Target) {
        IG_LOG(L_WARNING) << "Switched from "
                          << targetToString(lopts.Target) << " to "
                          << targetToString(newTarget) << std::endl;
    }
    mTarget = newTarget;

    IG_LOG(L_INFO) << "Loading target " << targetToString(newTarget) << std::endl;
    if (!mManager.load(newTarget, mLoadedInterface))
        throw std::runtime_error("Error loading interface!");

    if (opts.SPI == 0)
        mSamplesPerIteration = recommendSPI(mTarget);
    else
        mSamplesPerIteration = opts.SPI;

    lopts.Target              = mTarget;
    lopts.SamplesPerIteration = mSamplesPerIteration;
    IG_LOG(L_DEBUG) << "Recommended samples per iteration = " << mSamplesPerIteration << std::endl;

    IG_LOG(L_DEBUG) << "Loading scene" << std::endl;
    LoaderResult result;
    if (!Loader::load(lopts, result))
        throw std::runtime_error("Could not load scene!");
    mDatabase = std::move(result.Database);

    // Pickup initial camera view
    setup_camera_view(mLoadedRenderSettings, lopts, mDatabase.SceneBBox);

    mIsDebug       = lopts.TechniqueType == "debug";
    mIsTrace       = lopts.CameraType == "list";
    mTechniqueInfo = result.TechniqueInfo;

    mTechniqueVariants = std::move(result.TechniqueVariants);

    if (opts.DumpShader) {
        for (size_t i = 0; i < mTechniqueVariants.size(); ++i) {
            const auto& variant = mTechniqueVariants[i];
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

    // Force flush to zero mode for denormals
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
    _mm_setcsr(_mm_getcsr() | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
#endif
}

Runtime::~Runtime()
{
    if (mInit)
        shutdown();
}

void Runtime::step(const Camera& camera)
{
    if (!mInit)
        setup();

    if (mIsTrace) {
        IG_LOG(L_ERROR) << "Trying to use step() in a trace driver!" << std::endl;
        return;
    }

    if (mTechniqueInfo.VariantSelector) {
        const auto& active = mTechniqueInfo.VariantSelector(mCurrentIteration);
        for (const auto& ind : active)
            stepVariant(camera, ind);
    } else {
        for (size_t i = 0; i < mTechniqueVariants.size(); ++i)
            stepVariant(camera, (int)i);
    }

    ++mCurrentIteration;
}

void Runtime::stepVariant(const Camera& camera, int variant)
{
    IG_ASSERT(variant < mTechniqueVariants.size(), "Expected technique variant to be well selected");
    const auto& info = mTechniqueInfo.Variants[variant];

    // IG_LOG(L_DEBUG) << "Rendering iteration " << mCurrentIteration << ", variant " << variant << std::endl;

    DriverRenderSettings settings;
    for (int i = 0; i < 3; ++i)
        settings.eye[i] = camera.Eye(i);
    for (int i = 0; i < 3; ++i)
        settings.dir[i] = camera.Direction(i);
    for (int i = 0; i < 3; ++i)
        settings.up[i] = camera.Up(i);
    for (int i = 0; i < 3; ++i)
        settings.right[i] = camera.Right(i);
    settings.width              = camera.SensorWidth;
    settings.height             = camera.SensorHeight;
    settings.tmin               = camera.TMin;
    settings.tmax               = camera.TMax;
    settings.rays               = nullptr; // No artifical ray streams
    settings.device             = mDevice;
    settings.spi                = info.GetSPI(mSamplesPerIteration);
    settings.work_width         = info.GetWidth(mLoadedRenderSettings.FilmWidth);
    settings.work_height        = info.GetHeight(mLoadedRenderSettings.FilmHeight);
    settings.framebuffer_locked = info.LockFramebuffer;
    settings.debug_mode         = (uint32)mDebugMode;

    mLoadedInterface.RenderFunction(mTechniqueVariantShaderSets[variant], settings, mCurrentIteration);

    if (!info.LockFramebuffer)
        mCurrentSampleCount += settings.spi;
}

void Runtime::trace(const std::vector<Ray>& rays, std::vector<float>& data)
{
    if (!mInit)
        setup();

    if (!mIsTrace) {
        IG_LOG(L_ERROR) << "Trying to use trace() in a camera driver!" << std::endl;
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
    IG_ASSERT(variant < mTechniqueVariants.size(), "Expected technique variant to be well selected");
    const auto& info = mTechniqueInfo.Variants[variant];

    // IG_LOG(L_DEBUG) << "Tracing iteration " << mCurrentIteration << ", variant " << variant << std::endl;

    DriverRenderSettings settings;
    settings.width              = rays.size();
    settings.height             = 1;
    settings.rays               = rays.data();
    settings.device             = mDevice;
    settings.spi                = info.GetSPI(mSamplesPerIteration);
    settings.work_width         = rays.size();
    settings.work_height        = 1;
    settings.framebuffer_locked = info.LockFramebuffer;

    mLoadedInterface.RenderFunction(mTechniqueVariantShaderSets[variant], settings, mCurrentIteration);

    if (!info.LockFramebuffer)
        mCurrentSampleCount += settings.spi;
}

void Runtime::resizeFramebuffer(size_t width, size_t height)
{
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

void Runtime::setup()
{
    DriverSetupSettings settings;
    settings.database           = &mDatabase;
    settings.framebuffer_width  = std::max(1u, mLoadedRenderSettings.FilmWidth);
    settings.framebuffer_height = std::max(1u, mLoadedRenderSettings.FilmHeight);
    settings.acquire_stats      = mAcquireStats;
    settings.aov_count          = mTechniqueInfo.EnabledAOVs.size();

    settings.logger = &IG_LOGGER;

    IG_LOG(L_DEBUG) << "Init JIT compiling" << std::endl;
    ig_init_jit(mManager.getPath(mTarget).generic_u8string());
    mLoadedInterface.SetupFunction(settings);

    compileShaders();
    mInit = true;

    clearFramebuffer();
}

void Runtime::shutdown()
{
    mLoadedInterface.ShutdownFunction();
}

void Runtime::compileShaders()
{
    mTechniqueVariantShaderSets.resize(mTechniqueVariants.size());
    for (size_t i = 0; i < mTechniqueVariants.size(); ++i) {
        const auto& variant = mTechniqueVariants[i];
        auto& shaders       = mTechniqueVariantShaderSets[i];

        IG_LOG(L_DEBUG) << "Handling technique variant " << i << std::endl;
        IG_LOG(L_DEBUG) << "Compiling ray generation shader" << std::endl;
        const std::filesystem::path rgp = "v" + std::to_string(i) + "_rayGenerationFull.art";
        shaders.RayGenerationShader     = ig_compile_source(variant.RayGenerationShader, "ig_ray_generation_shader",
                                                        mOptions.DumpShaderFull ? &rgp : nullptr);

        IG_LOG(L_DEBUG) << "Compiling miss shader" << std::endl;
        const std::filesystem::path mp = "v" + std::to_string(i) + "_missShaderFull.art";
        shaders.MissShader             = ig_compile_source(variant.MissShader, "ig_miss_shader",
                                               mOptions.DumpShaderFull ? &mp : nullptr);

        IG_LOG(L_DEBUG) << "Compiling hit shaders" << std::endl;
        for (size_t j = 0; j < variant.HitShaders.size(); ++j) {
            IG_LOG(L_DEBUG) << "Hit shader [" << j << "]" << std::endl;
            const std::filesystem::path hp = "v" + std::to_string(i) + "_hitShaderFull" + std::to_string(j) + ".art";
            shaders.HitShaders.push_back(ig_compile_source(variant.HitShaders[j], "ig_hit_shader", mOptions.DumpShaderFull ? &hp : nullptr));
        }

        if (!variant.AdvancedShadowHitShader.empty()) {
            IG_LOG(L_DEBUG) << "Compiling advanced shadow shaders" << std::endl;
            const std::filesystem::path ash = "v" + std::to_string(i) + "_advancedShadowHitFull.art";
            shaders.AdvancedShadowHitShader = ig_compile_source(variant.AdvancedShadowHitShader, "ig_advanced_shadow_shader",
                                                                mOptions.DumpShaderFull ? &ash : nullptr);

            const std::filesystem::path asm_ = "v" + std::to_string(i) + "_advancedShadowMissFull.art";
            shaders.AdvancedShadowMissShader = ig_compile_source(variant.AdvancedShadowMissShader, "ig_advanced_shadow_shader",
                                                                 mOptions.DumpShaderFull ? &asm_ : nullptr);
        }

        for (size_t i = 0; i < variant.CallbackShaders.size(); ++i) {
            if (variant.CallbackShaders[i].empty()) {
                shaders.CallbackShaders[i] = nullptr;
            } else {
                IG_LOG(L_DEBUG) << "Compiling callback shader [" << i << "]" << std::endl;
                const std::filesystem::path asm_ = " v" + std::to_string(i) + "_callbackFull" + std::to_string(i) + ".art";
                shaders.CallbackShaders[i]       = ig_compile_source(variant.CallbackShaders[i], "ig_callback_shader",
                                                               mOptions.DumpShaderFull ? &asm_ : nullptr);
            }
        }
    }
}

void Runtime::tonemap(uint32* out_pixels, const TonemapSettings& settings)
{
    if (!mInit)
        setup();

    mLoadedInterface.TonemapFunction(mDevice, out_pixels, settings);
}

void Runtime::imageinfo(const ImageInfoSettings& settings, ImageInfoOutput& output)
{
    if (!mInit)
        setup();

    mLoadedInterface.ImageInfoFunction(mDevice, settings, output);
}

} // namespace IG