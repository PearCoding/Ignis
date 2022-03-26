#include "Runtime.h"
#include "Logger.h"
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

static inline size_t recommendSPI(Target target, size_t width, size_t height)
{
    // The "best" case was measured with a 1000 x 1000. It does dependent on the scene content though, but thats ignored here
    const size_t spi_f = isCPU(target) ? 2 : 8;
    const size_t spi   = (size_t)std::ceil(spi_f / ((width / 1000.0f) * (height / 1000.0f)));
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
    , mFilmWidth(0)
    , mFilmHeight(0)
    , mInitialCameraOrientation()
    , mIsTrace(opts.IsTracer)
    , mAcquireStats(opts.AcquireStats)
    , mTechniqueName()
    , mTechniqueInfo()
    , mTechniqueVariants()
    , mTechniqueVariantShaderSets()
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

    // Load standard library if necessary
    if (!mOptions.ScriptDir.empty()) {
        IG_LOG(L_INFO) << "Loading standard library from " << mOptions.ScriptDir << std::endl;
        mScriptPreprocessor.loadStdLibFromDirectory(mOptions.ScriptDir);
    }

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
        mSamplesPerIteration = recommendSPI(mTarget, mFilmWidth, mFilmHeight);
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

    mTechniqueName            = lopts.TechniqueType;
    mTechniqueInfo            = result.TechniqueInfo;
    mInitialCameraOrientation = result.CameraOrientation;
    mTechniqueVariants        = std::move(result.TechniqueVariants);

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

void Runtime::stepVariant(size_t variant)
{
    IG_ASSERT(variant < mTechniqueVariants.size(), "Expected technique variant to be well selected");
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
            traceVariant(rays, i);
    }

    ++mCurrentIteration;

    // Get result
    const float* data_ptr = getFramebuffer(0);
    data.resize(rays.size() * 3);
    std::memcpy(data.data(), data_ptr, sizeof(float) * rays.size() * 3);
}

void Runtime::traceVariant(const std::vector<Ray>& rays, size_t variant)
{
    IG_ASSERT(variant < mTechniqueVariants.size(), "Expected technique variant to be well selected");
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

const float* Runtime::getFramebuffer(size_t aov) const
{
    return mLoadedInterface.GetFramebufferFunction(aov);
}

void Runtime::clearFramebuffer()
{
    return mLoadedInterface.ClearFramebufferFunction(-1);
}

void Runtime::clearFramebuffer(size_t aov)
{
    return mLoadedInterface.ClearFramebufferFunction((int)aov);
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
    const std::string driver_filename = mManager.getPath(mTarget).generic_u8string();

    DriverSetupSettings settings;
    settings.driver_filename    = driver_filename.c_str();
    settings.database           = &mDatabase;
    settings.framebuffer_width  = (uint32)mFilmWidth;
    settings.framebuffer_height = (uint32)mFilmHeight;
    settings.acquire_stats      = mAcquireStats;
    settings.aov_count          = mTechniqueInfo.EnabledAOVs.size();

    settings.logger = &IG_LOGGER;

    IG_LOG(L_DEBUG) << "Init driver" << std::endl;
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
        shaders.RayGenerationShader = compileShader(variant.RayGenerationShader, "ig_ray_generation_shader", "v" + std::to_string(i) + "_rayGeneration");
        if (shaders.RayGenerationShader == nullptr) {
            IG_LOG(L_ERROR) << "Failed to compile ray generation shader in variant " << i << "." << std::endl;
            return false;
        }

        IG_LOG(L_DEBUG) << "Compiling miss shader" << std::endl;
        shaders.MissShader = compileShader(variant.MissShader, "ig_miss_shader", "v" + std::to_string(i) + "_missShader");
        if (shaders.MissShader == nullptr) {
            IG_LOG(L_ERROR) << "Failed to compile miss shader in variant " << i << "." << std::endl;
            return false;
        }

        IG_LOG(L_DEBUG) << "Compiling hit shaders" << std::endl;
        for (size_t j = 0; j < variant.HitShaders.size(); ++j) {
            IG_LOG(L_DEBUG) << "Hit shader [" << j << "]" << std::endl;
            shaders.HitShaders.push_back(compileShader(variant.HitShaders[j].c_str(), "ig_hit_shader", "v" + std::to_string(i) + "_hitShader"));
            if (shaders.HitShaders[j] == nullptr) {
                IG_LOG(L_ERROR) << "Failed to compile hit shader " << j << " in variant " << i << "." << std::endl;
                return false;
            }
        }

        if (!variant.AdvancedShadowHitShader.empty()) {
            IG_LOG(L_DEBUG) << "Compiling advanced shadow shaders" << std::endl;
            shaders.AdvancedShadowHitShader = compileShader(variant.AdvancedShadowHitShader.c_str(), "ig_advanced_shadow_shader", "v" + std::to_string(i) + "_advancedShadowHit");

            if (shaders.AdvancedShadowHitShader == nullptr) {
                IG_LOG(L_ERROR) << "Failed to compile advanced shadow hit shader in variant " << i << "." << std::endl;
                return false;
            }

            shaders.AdvancedShadowMissShader = compileShader(variant.AdvancedShadowMissShader.c_str(), "ig_advanced_shadow_shader", "v" + std::to_string(i) + "_advancedShadowMiss");

            if (shaders.AdvancedShadowMissShader == nullptr) {
                IG_LOG(L_ERROR) << "Failed to compile advanced shadow miss shader in variant " << i << "." << std::endl;
                return false;
            }
        }

        for (size_t j = 0; j < variant.CallbackShaders.size(); ++j) {
            if (variant.CallbackShaders.at(j).empty()) {
                shaders.CallbackShaders[j] = nullptr;
            } else {
                IG_LOG(L_DEBUG) << "Compiling callback shader [" << i << "]" << std::endl;
                shaders.CallbackShaders[j] = compileShader(variant.CallbackShaders.at(j), "ig_callback_shader", "v" + std::to_string(i) + "_callback" + std::to_string(j));
                if (shaders.CallbackShaders.at(j) == nullptr) {
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

    return mLoadedInterface.CompileSourceFunction(full_shader.c_str(), func.c_str());
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

} // namespace IG