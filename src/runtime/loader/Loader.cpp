#include "Loader.h"
#include "LoaderBSDF.h"
#include "LoaderCamera.h"
#include "LoaderEntity.h"
#include "LoaderLight.h"
#include "LoaderMedium.h"
#include "LoaderShape.h"
#include "LoaderTechnique.h"
#include "LoaderTexture.h"
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
std::optional<LoaderContext> Loader::load(const LoaderOptions& opts)
{
    LoaderContext ctx;
    ctx.Options = opts;

    // TODO: Combine both cache types?
    ctx.Cache        = std::make_shared<LoaderCache>();
    ctx.CacheManager = std::make_shared<IG::CacheManager>(opts.CachePath);
    ctx.CacheManager->enable(opts.EnableCache);

    ctx.Textures  = std::make_shared<LoaderTexture>();
    ctx.Lights    = std::make_unique<LoaderLight>();
    ctx.BSDFs     = std::make_unique<LoaderBSDF>();
    ctx.Media     = std::make_unique<LoaderMedium>();
    ctx.Shapes    = std::make_unique<LoaderShape>();
    ctx.Entities  = std::make_unique<LoaderEntity>();
    ctx.Technique = std::make_unique<LoaderTechnique>();
    ctx.Camera    = std::make_unique<LoaderCamera>();

    ctx.CacheManager->sync();

    ctx.Shapes->prepare(ctx);
    ctx.Entities->prepare(ctx);
    ctx.Textures->prepare(ctx);
    ctx.Lights->prepare(ctx);
    ctx.BSDFs->prepare(ctx);
    ctx.Media->prepare(ctx);

    // Load content

    ctx.CacheManager->sync();
    if (!ctx.Shapes->load(ctx))
        return std::nullopt;

    ctx.CacheManager->sync();
    if (!ctx.Entities->load(ctx))
        return std::nullopt;

    ctx.CacheManager->sync();
    ctx.Lights->setup(ctx);

    ctx.CacheManager->sync();

    IG_LOG(L_DEBUG) << "Got " << ctx.Materials.size() << " unique materials" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Lights->lightCount() << " lights" << std::endl;
    if (ctx.Lights->embeddedLightCount() > 0)
        IG_LOG(L_DEBUG) << "Got " << ctx.Lights->embeddedLightCount() << " embedded lights" << std::endl;
    if (ctx.Lights->areaLightCount() > 0)
        IG_LOG(L_DEBUG) << "Got " << ctx.Lights->areaLightCount() << " area lights" << std::endl;
    if (ctx.Media->mediumCount() > 0)
        IG_LOG(L_DEBUG) << "Got " << ctx.Media->mediumCount() << " participating media" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Shapes->shapeCount() << " shapes" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Shapes->triShapeCount() << " triangular shapes" << std::endl;
    if (ctx.Shapes->planeShapeCount() > 0)
        IG_LOG(L_DEBUG) << "Got " << ctx.Shapes->planeShapeCount() << " shapes which are approximative planar" << std::endl;
    if (ctx.Shapes->sphereShapeCount() > 0)
        IG_LOG(L_DEBUG) << "Got " << ctx.Shapes->sphereShapeCount() << " shapes which are approximative spherical" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Entities->entityCount() << " entities" << std::endl;

    ctx.Database.MaterialCount = ctx.Materials.size(); // TODO: Refactor this

    ctx.Camera->setup(ctx);
    if (!ctx.Camera->hasCamera())
        return std::nullopt;

    ctx.Technique->setup(ctx);
    if (!ctx.Technique->hasTechnique())
        return std::nullopt;

    if (ctx.Technique->info().Variants.empty()) {
        IG_LOG(L_ERROR) << "Invalid technique with no variants" << std::endl;
        return std::nullopt;
    }

    if (ctx.Technique->info().Variants.size() == 1)
        IG_LOG(L_DEBUG) << "Generating shaders for a single variant" << std::endl;
    else
        IG_LOG(L_DEBUG) << "Generating shaders for " << ctx.Technique->info().Variants.size() << " variants" << std::endl;

    ctx.TechniqueVariants.resize(ctx.Technique->info().Variants.size());
    try {
        for (size_t i = 0; i < ctx.Technique->info().Variants.size(); ++i) {
            const auto setup = [&](const std::string& name, const std::function<std::string()>& func, ShaderOutput<std::string>& output) {
                ctx.resetLocalRegistry();
                IG_LOG(L_DEBUG) << "Generating " << name << " shader for variant " << i << std::endl;
                output.Exec = func();
                if (output.Exec.empty()) {
                    throw std::runtime_error("Constructed empty " + name + " shader.");
                }
                output.LocalRegistry = std::move(ctx.LocalRegistry);
            };

            auto& variant                   = ctx.TechniqueVariants[i];
            const auto& info                = ctx.Technique->info().Variants[i];
            ctx.CurrentTechniqueVariant     = i;
            ctx.Options.SamplesPerIteration = info.GetSPI(opts.SamplesPerIteration);

            // Generate shaders
            setup(
                "device", [&]() { return DeviceShader::setup(ctx); }, variant.DeviceShader);
            setup(
                "tonemap", [&]() { return UtilityShader::setupTonemap(ctx); }, variant.TonemapShader);
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
            for (size_t j = 0; j < ctx.Materials.size(); ++j) {
                ShaderOutput<std::string> output;
                setup(
                    "hit " + std::to_string(j), [&]() { return HitShader::setup(j, ctx); }, output);
                variant.HitShaders.emplace_back(std::move(output));
            }

            // Generate advanced shadow shaders if requested
            if (info.ShadowHandlingMode != ShadowHandlingMode::Simple) {
                const size_t max_materials = info.ShadowHandlingMode == ShadowHandlingMode::Advanced ? 1 : ctx.Materials.size();
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
        return std::nullopt;
    }

    // Put some information into the registry
    ctx.GlobalRegistry.IntParameters["__entity_count"]        = (int)ctx.Entities->entityCount();
    ctx.GlobalRegistry.IntParameters["__shape_count"]         = (int)ctx.Shapes->shapeCount();
    ctx.GlobalRegistry.IntParameters["__material_count"]      = (int)ctx.Materials.size();
    ctx.GlobalRegistry.VectorParameters["__scene_bbox_lower"] = ctx.SceneBBox.min;
    ctx.GlobalRegistry.VectorParameters["__scene_bbox_upper"] = ctx.SceneBBox.max;

    if (ctx.HasError)
        return std::nullopt;
    else
        return std::optional<LoaderContext>(std::in_place, std::move(ctx));
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