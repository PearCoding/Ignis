#include "Loader.h"
#include "LoaderBSDF.h"
#include "LoaderCamera.h"
#include "LoaderEntity.h"
#include "LoaderLight.h"
#include "LoaderShape.h"
#include "LoaderTechnique.h"
#include "Logger.h"
#include "shader/AdvancedShadowShader.h"
#include "shader/DeviceShader.h"
#include "shader/HitShader.h"
#include "shader/MissShader.h"
#include "shader/RayGenerationShader.h"
#include "shader/TraversalShader.h"
#include "shader/UtilityShader.h"

#include <chrono>
#include <functional>

namespace IG {
bool Loader::load(const LoaderOptions& opts, LoaderResult& result)
{
    LoaderContext ctx;
    ctx.Database            = &result.Database;
    ctx.FilePath            = opts.FilePath;
    ctx.Target              = opts.Target;
    ctx.Scene               = opts.Scene;
    ctx.CameraType          = opts.CameraType;
    ctx.TechniqueType       = opts.TechniqueType;
    ctx.PixelSamplerType    = opts.PixelSamplerType;
    ctx.SamplesPerIteration = opts.SamplesPerIteration;
    ctx.IsTracer            = opts.IsTracer;
    ctx.Denoiser            = opts.Denoiser;
    ctx.FilmWidth           = opts.FilmWidth;
    ctx.FilmHeight          = opts.FilmHeight;

    ctx.Lights   = std::make_unique<LoaderLight>();
    ctx.Shapes   = std::make_unique<LoaderShape>();
    ctx.Entities = std::make_unique<LoaderEntity>();

    ctx.ForceShadingTreeSpecialization = opts.ForceSpecialization;

    ctx.Shapes->prepare(ctx);
    ctx.Entities->prepare(ctx);
    ctx.Lights->prepare(ctx);

    // Load content
    if (!ctx.Shapes->load(ctx, result))
        return false;

    if (!ctx.Entities->load(ctx, result))
        return false;

    LoaderCamera::setupInitialOrientation(ctx, result);

    ctx.Lights->setup(ctx);
    IG_LOG(L_DEBUG) << "Got " << ctx.Environment.Materials.size() << " unique materials" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Lights->lightCount() << " lights" << std::endl;
    if (ctx.Lights->embeddedLightCount() > 0)
        IG_LOG(L_DEBUG) << "Got " << ctx.Lights->embeddedLightCount() << " embedded lights" << std::endl;
    if (ctx.Lights->areaLightCount() > 0)
        IG_LOG(L_DEBUG) << "Got " << ctx.Lights->areaLightCount() << " area lights" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Shapes->shapeCount() << " shapes" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Shapes->triShapeCount() << " triangular shapes" << std::endl;
    if (ctx.Shapes->planeShapeCount() > 0)
        IG_LOG(L_DEBUG) << "Got " << ctx.Shapes->planeShapeCount() << " shapes which are approximative planar" << std::endl;
    if (ctx.Shapes->sphereShapeCount() > 0)
        IG_LOG(L_DEBUG) << "Got " << ctx.Shapes->sphereShapeCount() << " shapes which are approximative spherical" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Entities->entityCount() << " entities" << std::endl;

    result.Database.MaterialCount = ctx.Environment.Materials.size();

    auto tech_info = LoaderTechnique::getInfo(ctx);
    if (!tech_info.has_value())
        return false;

    ctx.TechniqueInfo = tech_info.value();

    if (ctx.TechniqueInfo.Variants.empty()) {
        IG_LOG(L_ERROR) << "Invalid technique with no variants" << std::endl;
        return false;
    }

    if (ctx.TechniqueInfo.Variants.size() == 1)
        IG_LOG(L_DEBUG) << "Generating shaders for a single variant" << std::endl;
    else
        IG_LOG(L_DEBUG) << "Generating shaders for " << ctx.TechniqueInfo.Variants.size() << " variants" << std::endl;

    result.TechniqueVariants.resize(ctx.TechniqueInfo.Variants.size());
    try {
        for (size_t i = 0; i < ctx.TechniqueInfo.Variants.size(); ++i) {
            const auto setup = [&](const std::string& name, const std::function<std::string()>& func, ShaderOutput<std::string>& output) {
                ctx.resetRegistry();
                IG_LOG(L_DEBUG) << "Generating " << name << " shader for variant " << i << std::endl;
                output.Exec = func();
                if (output.Exec.empty()) {
                    throw std::runtime_error("Constructed empty " + name + " shader.");
                }
                output.LocalRegistry = std::move(ctx.LocalRegistry);
            };

            auto& variant               = result.TechniqueVariants[i];
            const auto& info            = ctx.TechniqueInfo.Variants[i];
            ctx.CurrentTechniqueVariant = i;
            ctx.SamplesPerIteration     = info.GetSPI(opts.SamplesPerIteration);

            // Generate shaders
            setup(
                "device", [&]() { return DeviceShader::setup(ctx); }, variant.DeviceShader);
            setup(
                "tonemap", [&]() { return UtilityShader::setupTonemap(ctx); }, variant.TonemapShader);
            setup(
                "glare", [&]() { return UtilityShader::setupGlare(ctx); }, variant.GlareShader);
            setup(
                "imageinfo", [&]() { return UtilityShader::setupImageinfo(ctx); }, variant.ImageinfoShader);
            setup(
                "primary traversal", [&]() { return TraversalShader::setupPrimary(ctx); }, variant.PrimaryTraversalShader);
            setup(
                "secondary traversal", [&]() { return TraversalShader::setupSecondary(ctx); }, variant.SecondaryTraversalShader);
            setup(
                "ray generation", [&]() { return info.OverrideCameraGenerator ? info.OverrideCameraGenerator(ctx) : RayGenerationShader::setup(ctx); }, variant.RayGenerationShader);
            setup(
                "miss", [&]() { return MissShader::setup(ctx); }, variant.MissShader);

            // Generate hit shaders
            for (size_t j = 0; j < ctx.Environment.Materials.size(); ++j) {
                ShaderOutput<std::string> output;
                setup(
                    "hit " + std::to_string(j), [&]() { return HitShader::setup(j, ctx); }, output);
                variant.HitShaders.emplace_back(std::move(output));
            }

            // Generate advanced shadow shaders if requested
            if (info.ShadowHandlingMode != ShadowHandlingMode::Simple) {
                const size_t max_materials = info.ShadowHandlingMode == ShadowHandlingMode::Advanced ? 1 : ctx.Environment.Materials.size();
                for (size_t j = 0; j < max_materials; ++j) {
                    ShaderOutput<std::string> output;
                    setup(
                        "advanced shadow hit " + std::to_string(j), [&]() { return AdvancedShadowShader::setup(true, j, ctx); }, output);
                    variant.AdvancedShadowHitShaders.emplace_back(std::move(output));
                }
                for (size_t j = 0; j < max_materials; ++j) {
                    ShaderOutput<std::string> output;
                    setup(
                        "advanced shadow miss " + std::to_string(j), [&]() { return AdvancedShadowShader::setup(false, j, ctx); }, output);
                    variant.AdvancedShadowMissShaders.emplace_back(std::move(output));
                }
            }

            // Generate callback shaders if requested
            for (size_t j = 0; j < info.CallbackGenerators.size(); ++j) {
                if (info.CallbackGenerators.at(j) != nullptr) {
                    setup(
                        "callback " + std::to_string(j), [&]() { return info.CallbackGenerators.at(j)(ctx); }, variant.CallbackShaders[j]);
                }
            }
        }
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        return false;
    }

    result.Database.SceneRadius = ctx.Environment.SceneDiameter / 2.0f;
    result.Database.SceneBBox   = ctx.Environment.SceneBBox;
    result.TechniqueInfo        = ctx.TechniqueInfo;
    result.ResourceMap          = ctx.generateResourceMap();

    return !ctx.HasError;
}

std::vector<std::string> Loader::getAvailableTechniqueTypes()
{
    return LoaderTechnique::getAvailableTypes();
}

std::vector<std::string> Loader::getAvailableCameraTypes()
{
    return LoaderCamera::getAvailableTypes();
}
} // namespace IG