#include "Runtime.h"
#include "Image.h"
#include "Logger.h"
#include "RuntimeInfo.h"
#include "StringUtils.h"
#include "device/DeviceManager.h"
#include "device/IDeviceInterface.h"
#include "loader/LoaderCamera.h"
#include "loader/Parser.h"
#include "shader/ShaderManager.h"

#include <chrono>
#include <fstream>
#include <thread>

IG_BEGIN_IGNORE_WARNINGS
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
IG_END_IGNORE_WARNINGS

namespace IG {

static inline void setup_technique(LoaderOptions& lopts, const RuntimeOptions& opts)
{
    std::string tech_type;
    if (opts.OverrideTechnique.empty()) {
        const auto technique = lopts.Scene->technique();
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
    const auto film   = lopts.Scene->film();
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
        const auto camera = lopts.Scene->camera();
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

Runtime::Runtime(const RuntimeOptions& opts)
    : mOptions(opts)
    , mDatabase()
    , mGlobalRegistry()
    , mSamplesPerIteration(0)
    , mCurrentIteration(0)
    , mCurrentSampleCount(0)
    , mCurrentFrame(0)
    , mFilmWidth(0)
    , mFilmHeight(0)
    , mCameraName()
    , mInitialCameraOrientation()
    , mTechniqueName()
    , mTechniqueInfo()
    , mTechniqueVariants()
    , mTechniqueVariantShaderSets()
{
    checkCacheDirectory();

    // Get device
    if (!DeviceManager::instance().init(/*TODO Parameter*/))
        throw std::runtime_error("Could not initialize devices");

    const IDeviceInterface* interface = DeviceManager::instance().getDevice(mOptions.Target.architecture());
    if (interface == nullptr)
        throw std::runtime_error("Could not get requested device: " + mOptions.Target.toString());

    // Get compiler interface
    std::shared_ptr<ICompilerDevice> compilerDevice = std::shared_ptr<ICompilerDevice>{ interface->createCompilerDevice() };
    if (compilerDevice == nullptr)
        throw std::runtime_error("Could not get compiler interface from requested device");

    // Configure compiler
    mCompiler = std::make_unique<ScriptCompiler>(compilerDevice);

    mCompiler->setOptimizationLevel(std::min<size_t>(3, mOptions.ShaderOptimizationLevel));
    mCompiler->setVerbose(IG_LOGGER.verbosity() == L_DEBUG);

    // Check configuration
    if (!mOptions.Target.isValid())
        throw std::runtime_error("Could not pick a suitable target");

    // Load interface
    IG_LOG(L_INFO) << "Using target " << mOptions.Target.toString() << std::endl;

    // Load standard library if necessary
    if (!mOptions.ScriptDir.empty()) {
        IG_LOG(L_INFO) << "Loading standard library from " << mOptions.ScriptDir << std::endl;
        mCompiler->loadStdLibFromDirectory(mOptions.ScriptDir);
    }

    IRenderDevice::SetupSettings settings;
    settings.target        = mOptions.Target;
    settings.AcquireStats  = mOptions.AcquireStats;
    settings.DebugTrace    = mOptions.DebugTrace;
    settings.IsInteractive = mOptions.IsInteractive;

    IG_LOG(L_DEBUG) << "Init device" << std::endl;
    mDevice = std::unique_ptr<IRenderDevice>{ interface->createRenderDevice(settings) };
    if (mDevice == nullptr)
        throw std::runtime_error("Could not creater render interface from requested device");
}

Runtime::~Runtime()
{
}

void Runtime::checkCacheDirectory()
{
    constexpr size_t WarnSize = 1024 * 1024 * 1024 * 10ULL; // 10GB

    auto dir = RuntimeInfo::cacheDirectory();
    if (dir.empty())
        return;

    try {
        std::filesystem::create_directories(dir); // Make sure this directory exists
    } catch (const std::filesystem::filesystem_error&) {
        dir = std::filesystem::temp_directory_path() / dir.filename();
        IG_LOG(L_WARNING) << "Could not use " << RuntimeInfo::cacheDirectory() << " as jit cache. Trying to use " << dir << " instead." << std::endl;
        try {
            std::filesystem::create_directories(dir);
        } catch (const std::filesystem::filesystem_error&) {
            IG_LOG(L_ERROR) << "Could not use " << dir << " as jit cache. Proceeding might fail." << std::endl;
        }
    }

    const size_t size = RuntimeInfo::sizeOfDirectory(dir);
    if (size >= WarnSize)
        IG_LOG(L_WARNING) << "Cache directory " << dir << " occupies " << FormatMemory(size) << " of disk space and exceeds " << FormatMemory(WarnSize) << " warn limit " << std::endl;
    else
        IG_LOG(L_DEBUG) << "Cache directory " << dir << " occupies " << FormatMemory(size) << " of disk space" << std::endl;
}

bool Runtime::loadFromFile(const Path& path)
{
    // Parse scene file
    IG_LOG(L_DEBUG) << "Parsing scene file" << std::endl;
    try {
        const auto startParser = std::chrono::high_resolution_clock::now();
        SceneParser parser;
        auto scene = parser.loadFromFile(path);
        IG_LOG(L_DEBUG) << "Parsing scene took " << (std::chrono::high_resolution_clock::now() - startParser) << std::endl;
        if (scene == nullptr)
            return false;

        if (mOptions.AddExtraEnvLight)
            scene->addConstantEnvLight();

        return load(path, scene.get());
    } catch (const std::runtime_error& err) {
        IG_LOG(L_ERROR) << "Loading error: " << err.what() << std::endl;
        return false;
    }
}

bool Runtime::loadFromString(const std::string& str, const Path& dir)
{
    // Parse scene string
    try {
        IG_LOG(L_DEBUG) << "Parsing scene string" << std::endl;
        const auto startParser = std::chrono::high_resolution_clock::now();
        SceneParser parser;
        auto scene = parser.loadFromString(str, dir);
        IG_LOG(L_DEBUG) << "Parsing scene took " << (std::chrono::high_resolution_clock::now() - startParser) << std::endl;
        if (scene == nullptr)
            return false;

        if (mOptions.AddExtraEnvLight)
            scene->addConstantEnvLight();

        return load({}, scene.get());
    } catch (const std::runtime_error& err) {
        IG_LOG(L_ERROR) << "Loading error: " << err.what() << std::endl;
        return false;
    }
}

bool Runtime::loadFromScene(const Scene* scene)
{
    if (scene == nullptr) {
        IG_LOG(L_ERROR) << "Loading error: Given scene pointer is null" << std::endl;
        return false;
    }

    try {
        return load({}, scene);
    } catch (const std::runtime_error& err) {
        IG_LOG(L_ERROR) << "Loading error: " << err.what() << std::endl;
        return false;
    }
}

LoaderOptions Runtime::loaderOptions() const
{
    LoaderOptions lopts;
    lopts.FilePath            = Path{};
    lopts.EnableCache         = mOptions.EnableCache;
    lopts.CachePath           = mOptions.CacheDir;
    lopts.Target              = mOptions.Target;
    lopts.IsTracer            = mOptions.IsTracer;
    lopts.Scene               = nullptr;
    lopts.Specialization      = mOptions.Specialization;
    lopts.DisableStandardAOVs = mOptions.DisableStandardAOVs;
    lopts.EnableTonemapping   = mOptions.EnableTonemapping;
    lopts.Denoiser            = mOptions.Denoiser;
    lopts.Denoiser.Enabled    = !mOptions.IsTracer && mOptions.Denoiser.Enabled && hasDenoiser();
    lopts.Compiler            = mCompiler.get();
    lopts.Device              = mDevice.get();
    return lopts;
}

bool Runtime::load(const Path& path, const Scene* scene)
{
    LoaderOptions lopts = loaderOptions();
    lopts.FilePath      = path;
    lopts.Scene         = scene;
    lopts.CachePath     = mOptions.CacheDir.empty() ? (path.parent_path() / ("ignis_cache_" + path.stem().generic_string())) : mOptions.CacheDir;

    // Print a warning if denoiser was requested but none is available
    if (mOptions.Denoiser.Enabled && !lopts.Denoiser.Enabled && !mOptions.IsTracer && !hasDenoiser())
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
        mSamplesPerIteration = recommendSPI(mOptions.Target, mFilmWidth, mFilmHeight, mOptions.IsInteractive);
    else
        mSamplesPerIteration = mOptions.SPI;

    lopts.SamplesPerIteration = mSamplesPerIteration;
    IG_LOG(L_DEBUG) << "Recommended samples per iteration = " << mSamplesPerIteration << std::endl;

    IG_LOG(L_DEBUG) << "Loading scene" << std::endl;
    const auto startLoader = std::chrono::high_resolution_clock::now();

    auto ctx = Loader::load(lopts);
    if (!ctx)
        return false;
    mDatabase = std::move(ctx->Database);
    IG_LOG(L_DEBUG) << "Loading scene took " << (std::chrono::high_resolution_clock::now() - startLoader) << std::endl;

    mCameraName               = ctx->Options.CameraType;
    mTechniqueName            = ctx->Options.TechniqueType;
    mTechniqueInfo            = ctx->Technique->info();
    mInitialCameraOrientation = ctx->Camera->getOrientation(*ctx);
    mTechniqueVariants        = std::move(ctx->TechniqueVariants);
    mResourceMap              = ctx->generateResourceMap();
    mSceneParameterDesc       = ctx->SceneParameterDesc;

    if (mOptions.Denoiser.Enabled)
        mTechniqueInfo.EnabledAOVs.emplace_back("Denoised");

    // Setup array of number of entities per material
    mEntityPerMaterial.clear();
    mEntityPerMaterial.reserve(ctx->Materials.size());
    for (const auto& mat : ctx->Materials)
        mEntityPerMaterial.emplace_back((int)mat.Count);

    // Merge global registry
    mGlobalRegistry.mergeFrom(ctx->GlobalRegistry);

    // Free memory from loader context
    ctx.reset();

    // Preload camera orientation
    setCameraOrientation(mInitialCameraOrientation);
    bool res = setupScene();

    if (!res)
        return false;

    if (mOptions.WarnUnused)
        scene->warnUnusedProperties();

    if (mOptions.Denoiser.Enabled && !mOptions.DisableStandardAOVs)
        mDenoiser = std::make_unique<OIDN>(this);

    return true;
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

    handleTime();

    if (mTechniqueInfo.VariantSelector) {
        const auto active = mTechniqueInfo.VariantSelector(mCurrentIteration);

        IG_ASSERT(active.size() > 0, "Expected some variants to be returned by the technique variant selector");

        for (size_t i = 0; i < active.size(); ++i)
            stepVariant(active[i]);
    } else {
        for (size_t i = 0; i < mTechniqueVariants.size(); ++i)
            stepVariant((int)i);
    }

    if (mDenoiser && !ignoreDenoiser)
        mDenoiser->run(mDevice.get());

    ++mCurrentIteration;
}

void Runtime::stepVariant(size_t variant)
{
    IG_ASSERT(variant < mTechniqueVariants.size(), "Expected technique variant to be well selected");
    const auto& info = mTechniqueInfo.Variants[variant];

    // IG_LOG(L_DEBUG) << "Rendering iteration " << mCurrentIteration << ", variant " << variant << std::endl;

    IRenderDevice::RenderSettings settings;
    settings.rays      = nullptr; // No artificial ray streams
    settings.spi       = info.GetSPI(mSamplesPerIteration);
    settings.width     = info.GetWidth(mFilmWidth);
    settings.height    = info.GetHeight(mFilmHeight);
    settings.info      = info;
    settings.iteration = mCurrentIteration;
    settings.frame     = mCurrentFrame;
    settings.user_seed = mOptions.Seed;

    mDevice->render(mTechniqueVariantShaderSets.at(variant), settings, &mGlobalRegistry);

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

    handleTime();

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
    const float* data_ptr = getFramebufferForHost({}).Data;
    data.resize(rays.size() * 3);
    std::memcpy(data.data(), data_ptr, sizeof(float) * data.size());
}

void Runtime::traceVariant(const std::vector<Ray>& rays, size_t variant)
{
    IG_ASSERT(variant < mTechniqueVariants.size(), "Expected technique variant to be well selected");
    const auto& info = mTechniqueInfo.Variants[variant];

    // IG_LOG(L_DEBUG) << "Tracing iteration " << mCurrentIteration << ", variant " << variant << std::endl;

    IRenderDevice::RenderSettings settings;
    settings.rays      = rays.data();
    settings.spi       = info.GetSPI(mSamplesPerIteration);
    settings.width     = rays.size();
    settings.height    = 1;
    settings.info      = info;
    settings.iteration = mCurrentIteration;
    settings.frame     = mCurrentFrame;
    settings.user_seed = mOptions.Seed;

    mDevice->render(mTechniqueVariantShaderSets.at(variant), settings, &mGlobalRegistry);

    if (!info.LockFramebuffer)
        mCurrentSampleCount += settings.spi;
}

void Runtime::resizeFramebuffer(size_t width, size_t height)
{
    mFilmWidth  = width;
    mFilmHeight = height;
    mDevice->resize(width, height);
    reset();
}

AOVAccessor Runtime::getFramebufferForHost(const std::string& name) const
{
    return mDevice->getFramebufferForHost(name);
}

AOVAccessor Runtime::getFramebufferForDevice(const std::string& name) const
{
    return mDevice->getFramebufferForDevice(name);
}

void Runtime::clearFramebuffer()
{
    mDevice->clearAllFramebuffer();
}

void Runtime::clearFramebuffer(const std::string& name)
{
    mDevice->clearFramebuffer(name);
}

size_t Runtime::getBufferSizeInBytes(const std::string& name) const
{
    return mDevice->getBufferSizeInBytes(name);
}

BufferAccessor Runtime::getBufferForDevice(const std::string& name) const
{
    return mDevice->getBufferForDevice(name);
}

bool Runtime::copyBufferToHost(const std::string& name, void* dst, size_t maxSizeInBytes)
{
    return mDevice->copyBufferToHost(name, dst, maxSizeInBytes);
}

void Runtime::reset()
{
    clearFramebuffer();
    mCurrentIteration   = 0;
    mCurrentSampleCount = 0;
    // No mCurrentFrameCount
}

const Statistics* Runtime::statistics() const
{
    return mOptions.AcquireStats ? mDevice->getStatistics() : nullptr;
}

static void dumpRegistries(std::ostream& stream, const std::string& name, const ShaderOutput<void*>& shader)
{
    if (shader.Exec == nullptr || shader.LocalRegistry == nullptr || shader.LocalRegistry->empty())
        return;

    stream << "Registry '" << name << "' at setup:" << std::endl
           << shader.LocalRegistry->dump();
}

static void dumpRegistries(std::ostream& stream, const TechniqueVariantBase<void*>& variant)
{
    dumpRegistries(stream, "Device", variant.DeviceShader);
    dumpRegistries(stream, "Tonemap", variant.TonemapShader);
    dumpRegistries(stream, "Imageinfo", variant.ImageinfoShader);
    dumpRegistries(stream, "PrimaryTraversal", variant.PrimaryTraversalShader);
    dumpRegistries(stream, "SecondaryTraversal", variant.SecondaryTraversalShader);
    dumpRegistries(stream, "RayGeneration", variant.RayGenerationShader);
    dumpRegistries(stream, "Miss", variant.MissShader);
    for (size_t i = 0; i < variant.HitShaders.size(); ++i)
        dumpRegistries(stream, "Hit[" + std::to_string(i) + "]", variant.HitShaders.at(i));
    for (size_t i = 0; i < variant.AdvancedShadowHitShaders.size(); ++i)
        dumpRegistries(stream, "AdvancedShadowHit[" + std::to_string(i) + "]", variant.AdvancedShadowHitShaders.at(i));
    for (size_t i = 0; i < variant.AdvancedShadowMissShaders.size(); ++i)
        dumpRegistries(stream, "AdvancedShadowMiss[" + std::to_string(i) + "]", variant.AdvancedShadowMissShaders.at(i));
    for (size_t i = 0; i < variant.CallbackShaders.size(); ++i)
        dumpRegistries(stream, "Callback[" + std::to_string(i) + "]", variant.CallbackShaders.at(i));
}

bool Runtime::setupScene()
{
    IRenderDevice::SceneSettings settings;
    settings.database            = &mDatabase;
    settings.aov_map             = &mTechniqueInfo.EnabledAOVs;
    settings.resource_map        = &mResourceMap;
    settings.entity_per_material = &mEntityPerMaterial;

    IG_LOG(L_DEBUG) << "Assign scene to device" << std::endl;
    mDevice->assignScene(settings);

    if (IG_LOGGER.verbosity() <= L_DEBUG) {
        if (mOptions.DumpRegistry) {
            if (mGlobalRegistry.empty()) {
                IG_LOG(L_DEBUG) << "Global registry at setup: None" << std::endl;
            } else {
                IG_LOG(L_DEBUG) << "Global registry at setup:" << std::endl
                                << mGlobalRegistry.dump();
            }
        }

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

    if (IG_LOGGER.verbosity() <= L_DEBUG) {
        if (mOptions.DumpRegistryFull) {
            for (size_t i = 0; i < mTechniqueVariantShaderSets.size(); ++i) {
                auto& stream = IG_LOG_UNSAFE(L_DEBUG);
                stream << "Local Variant [" << i << "] Registries:" << std::endl;
                dumpRegistries(stream, mTechniqueVariantShaderSets.at(i));
            }
        }
    }

    mDevice->releaseAll(); // Release all temporary stuff allocated while loading
    clearFramebuffer();
    return true;
}

bool Runtime::compileShaders()
{
    size_t threads = mOptions.ShaderCompileThreads;
    if (threads == 0)
        threads = std::thread::hardware_concurrency() / 2; // Using the compiler might be a heavy task, therefore only use half of the cpu count. TODO: Make this smarter?

    if (threads > 1) {
        if (RuntimeInfo::igcPath().empty()) {
            IG_LOG(L_WARNING) << "Could not find " << RuntimeInfo::igcPath() << ". Falling back to single threaded shader compilation" << std::endl;
            threads = 1;
        } else {
            IG_LOG(L_DEBUG) << "Using compiler at " << RuntimeInfo::igcPath() << std::endl;
        }
    }

    threads = std::max<size_t>(1, threads);

    ShaderManager manager;
    const auto registerShader = [&](size_t i, const std::string& name, const std::string& function, const ShaderOutput<std::string>* source, ShaderOutput<void*>* compiled) {
        const std::string id = "v" + std::to_string(i) + " " + name;
        manager.add(id,
                    ShaderManager::ShaderEntry{
                        .Source   = source,
                        .Function = function,
                        .Compiled = compiled,
                    });
    };

    // Register all types
    mTechniqueVariantShaderSets.resize(mTechniqueVariants.size());
    for (size_t i = 0; i < mTechniqueVariants.size(); ++i) {
        const auto& variant = mTechniqueVariants[i];
        auto& shaders       = mTechniqueVariantShaderSets[i];
        shaders.ID          = (uint32)i;

        registerShader(i, "device", "ig_callback_shader", &variant.DeviceShader, &shaders.DeviceShader);
        if (mOptions.EnableTonemapping) {
            registerShader(i, "tonemap", "ig_tonemap_shader", &variant.TonemapShader, &shaders.TonemapShader);
            registerShader(i, "imageinfo", "ig_imageinfo_shader", &variant.ImageinfoShader, &shaders.ImageinfoShader);
        }
        registerShader(i, "primary traversal", "ig_traversal_shader", &variant.PrimaryTraversalShader, &shaders.PrimaryTraversalShader);
        registerShader(i, "secondary traversal", "ig_traversal_shader", &variant.SecondaryTraversalShader, &shaders.SecondaryTraversalShader);
        registerShader(i, "ray generation", "ig_ray_generation_shader", &variant.RayGenerationShader, &shaders.RayGenerationShader);
        registerShader(i, "miss", "ig_miss_shader", &variant.MissShader, &shaders.MissShader);

        shaders.HitShaders.resize(variant.HitShaders.size());
        for (size_t j = 0; j < variant.HitShaders.size(); ++j)
            registerShader(i, "hit shader " + std::to_string(j), "ig_hit_shader", &variant.HitShaders[j], &shaders.HitShaders[j]);

        shaders.AdvancedShadowHitShaders.resize(variant.AdvancedShadowHitShaders.size());
        for (size_t j = 0; j < variant.AdvancedShadowHitShaders.size(); ++j)
            registerShader(i, "advanced shadow hit shader " + std::to_string(j), "ig_advanced_shadow_shader", &variant.AdvancedShadowHitShaders[j], &shaders.AdvancedShadowHitShaders[j]);

        shaders.AdvancedShadowMissShaders.resize(variant.AdvancedShadowMissShaders.size());
        for (size_t j = 0; j < variant.AdvancedShadowMissShaders.size(); ++j)
            registerShader(i, "advanced shadow miss shader " + std::to_string(j), "ig_advanced_shadow_shader", &variant.AdvancedShadowMissShaders[j], &shaders.AdvancedShadowMissShaders[j]);

        for (size_t j = 0; j < variant.CallbackShaders.size(); ++j) {
            if (variant.CallbackShaders[j].Exec.empty()) {
                shaders.CallbackShaders[j].Exec = nullptr;
            } else {
                registerShader(i, "callback " + std::to_string(j), "ig_callback_shader", &variant.CallbackShaders[j], &shaders.CallbackShaders[j]);
            }
        }
    }

    const auto startJIT = std::chrono::high_resolution_clock::now();
    const bool result   = manager.compile(mCompiler.get(), threads, mOptions.DumpShaderFull ? ShaderDumpVerbosity::Full : (mOptions.DumpShader ? ShaderDumpVerbosity::Light : ShaderDumpVerbosity::None));

    IG_LOG(L_DEBUG) << "Compiling shaders took " << (std::chrono::high_resolution_clock::now() - startJIT) << std::endl;

    return result;
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
    mGlobalRegistry.IntParameters[name] = value;
}

void Runtime::setParameter(const std::string& name, float value)
{
    mGlobalRegistry.FloatParameters[name] = value;
}

void Runtime::setParameter(const std::string& name, const Vector3f& value)
{
    mGlobalRegistry.VectorParameters[name] = value;
}

void Runtime::setParameter(const std::string& name, const Vector4f& value)
{
    mGlobalRegistry.ColorParameters[name] = value;
}

void Runtime::setParameter(const std::string& name, const std::string& value)
{
    mGlobalRegistry.StringParameters[name] = value;
}

void Runtime::mergeParametersFrom(const ParameterSet& other)
{
    mGlobalRegistry.mergeFrom(other, true);
}

std::vector<std::string> Runtime::getAvailableTechniqueTypes()
{
    return Loader::getAvailableTechniqueTypes();
}

std::vector<std::string> Runtime::getAvailableCameraTypes()
{
    return Loader::getAvailableCameraTypes();
}

void Runtime::setCameraOrientation(const CameraOrientation& orientation)
{
    setParameter("__camera_eye", orientation.Eye);
    setParameter("__camera_dir", orientation.Dir);
    setParameter("__camera_up", orientation.Up);
}

CameraOrientation Runtime::getCameraOrientation() const
{
    const Vector3f eye = mGlobalRegistry.VectorParameters.contains("__camera_eye") ? mGlobalRegistry.VectorParameters.at("__camera_eye") : Vector3f::Zero();
    const Vector3f dir = mGlobalRegistry.VectorParameters.contains("__camera_dir") ? mGlobalRegistry.VectorParameters.at("__camera_dir") : Vector3f::UnitZ();
    const Vector3f up  = mGlobalRegistry.VectorParameters.contains("__camera_up") ? mGlobalRegistry.VectorParameters.at("__camera_up") : Vector3f::UnitY();

    return CameraOrientation{
        .Eye = eye,
        .Dir = dir,
        .Up  = up
    };
}

bool Runtime::hasDenoiser()
{
    return OIDN::isAvailable();
}

void Runtime::handleTime()
{
    if (mCurrentIteration != 0)
        return;

    if (mCurrentFrame == 0) {
        mStartTime = std::chrono::high_resolution_clock::now();
        setParameter("__time", 0.0f);
    } else {
        const auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - mStartTime);
        setParameter("__time", (float)dur.count() / 1000.0f);
    }
}

std::shared_ptr<RenderPass> Runtime::createPass(const std::string& shaderCode)
{
    IG_ASSERT(mCompiler, "Expected a valid compiler");
    const std::string fullShader = mCompiler->prepare(shaderCode);
    void* callback               = mCompiler->compile(fullShader, "ig_pass_main");
    if (!callback)
        return nullptr;

    return std::make_shared<RenderPass>(this, callback);
}

bool Runtime::runPass(const RenderPass& pass)
{
    mDevice->runPass(ShaderOutput<void*>{
        .Exec          = pass.internalCallback(),
        .LocalRegistry = pass.parameter() });
    return true;
}

bool Runtime::saveFramebuffer(const Path& path) const
{
    const size_t width  = framebufferWidth();
    const size_t height = framebufferHeight();

    const auto& aovs       = this->aovs();
    const size_t aov_count = aovs.size() + 1;

    std::vector<float> images(width * height * 3 * aov_count);

    // Copy data
    for (size_t aov = 0; aov < aov_count; ++aov) {
        const std::string aov_name = aov == 0 ? std::string{} : aovs[aov - 1];

        float scale = currentIterationCount() > 0 ? 1.0f / currentIterationCount() : 1.0f;
        if (aov_name == "Normals" || aov_name == "Albedo")
            scale = 1; // TODO: Add flags for single use AOVs

        const auto acc   = getFramebufferForHost(aov_name);
        const float* src = acc.Data;
        float* dst_r     = &images[width * height * (3 * aov + 0)];
        float* dst_g     = &images[width * height * (3 * aov + 1)];
        float* dst_b     = &images[width * height * (3 * aov + 2)];

        const auto pixelF = [&](size_t ind) {
            float r = src[ind * 3 + 0];
            float g = src[ind * 3 + 1];
            float b = src[ind * 3 + 2];

            dst_r[ind] = r * scale;
            dst_g[ind] = g * scale;
            dst_b[ind] = b * scale;
        };

        tbb::parallel_for(tbb::blocked_range<size_t>(0, width * height),
                          [&](tbb::blocked_range<size_t> r) {
                              for (size_t i = r.begin(); i < r.end(); ++i)
                                  pixelF(i);
                          });
    }

    std::vector<const float*> image_ptrs(3 * aov_count);
    std::vector<std::string> image_names(3 * aov_count);
    for (size_t aov = 0; aov < aov_count; ++aov) {
        // Swizzle RGB to BGR as some viewers expect it per default
        image_ptrs[3 * aov + 2] = &images[width * height * (3 * aov + 0)];
        image_ptrs[3 * aov + 1] = &images[width * height * (3 * aov + 1)];
        image_ptrs[3 * aov + 0] = &images[width * height * (3 * aov + 2)];

        // Framebuffer
        if (aov == 0) {
            image_names[3 * aov + 0] = "B";
            image_names[3 * aov + 1] = "G";
            image_names[3 * aov + 2] = "R";
        } else {
            std::string name         = this->aovs()[aov - 1];
            image_names[3 * aov + 0] = name + ".B";
            image_names[3 * aov + 1] = name + ".G";
            image_names[3 * aov + 2] = name + ".R";
        }
    }

    // Populate meta data information
    ImageMetaData metaData;
    metaData.CameraType               = this->camera();
    metaData.TechniqueType            = this->technique();
    metaData.Seed                     = this->seed();
    metaData.SamplePerPixel           = this->currentSampleCount();
    metaData.SamplePerIteration       = this->samplesPerIteration();
    metaData.Iteration                = this->currentIterationCount();
    metaData.Frame                    = this->currentFrameCount();
    metaData.RendertimeInMilliseconds = metaData.Iteration > 0 ? (size_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - this->renderStartTime()).count() : (size_t)0;
    metaData.RendertimeInSeconds      = metaData.Iteration > 0 ? (size_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - this->renderStartTime()).count() : (size_t)0;
    metaData.TargetString             = this->target().toString();

    const CameraOrientation orientation = this->getCameraOrientation();

    metaData.CameraEye = orientation.Eye;
    metaData.CameraUp  = orientation.Up;
    metaData.CameraDir = orientation.Dir;

    return Image::save(path, width, height, image_ptrs, image_names, &metaData);
}
} // namespace IG