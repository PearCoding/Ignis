#include "Loader.h"
#include "LoaderBSDF.h"
#include "LoaderCamera.h"
#include "LoaderEntity.h"
#include "LoaderLight.h"
#include "LoaderShape.h"
#include "LoaderTechnique.h"
#include "Logger.h"
#include "shader/AdvancedShadowShader.h"
#include "shader/HitShader.h"
#include "shader/MissShader.h"
#include "shader/RayGenerationShader.h"

#include <chrono>

namespace IG {
bool Loader::load(const LoaderOptions& opts, LoaderResult& result)
{
    LoaderContext ctx;
    ctx.Database            = &result.Database;
    ctx.FilePath            = opts.FilePath;
    ctx.Target              = opts.Target;
    ctx.EnablePadding       = doesTargetRequirePadding(ctx.Target);
    ctx.Scene               = opts.Scene;
    ctx.CameraType          = opts.CameraType;
    ctx.TechniqueType       = opts.TechniqueType;
    ctx.PixelSamplerType    = opts.PixelSamplerType;
    ctx.SamplesPerIteration = opts.SamplesPerIteration;
    ctx.IsTracer            = opts.IsTracer;
    ctx.FilmWidth           = opts.FilmWidth;
    ctx.FilmHeight          = opts.FilmHeight;
    ctx.Lights              = std::make_unique<LoaderLight>();

    ctx.Lights->prepare(ctx);

    // Load content
    if (!LoaderShape::load(ctx, result))
        return false;

    if (!LoaderEntity::load(ctx, result))
        return false;

    LoaderCamera::setupInitialOrientation(ctx, result);

    IG_LOG(L_DEBUG) << "Got " << ctx.Environment.Materials.size() << " unique materials" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Lights->lightCount() << " lights" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Lights->embeddedLightCount() << " embedded lights" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Lights->areaLightCount() << " area lights" << std::endl;

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
    for (size_t i = 0; i < ctx.TechniqueInfo.Variants.size(); ++i) {
        auto& variant               = result.TechniqueVariants[i];
        const auto& info            = ctx.TechniqueInfo.Variants[i];
        ctx.CurrentTechniqueVariant = i;
        ctx.SamplesPerIteration     = info.GetSPI(opts.SamplesPerIteration);

        // Generate Ray Generation Shader
        IG_LOG(L_DEBUG) << "Generating ray generation shader for variant " << i << std::endl;
        if (info.OverrideCameraGenerator)
            variant.RayGenerationShader = info.OverrideCameraGenerator(ctx);
        else
            variant.RayGenerationShader = RayGenerationShader::setup(ctx);
        if (variant.RayGenerationShader.empty()) {
            IG_LOG(L_ERROR) << "Constructed empty ray generation shader." << std::endl;
            return false;
        }

        // Generate Miss Shader
        IG_LOG(L_DEBUG) << "Generating miss shader for variant " << i << std::endl;
        variant.MissShader = MissShader::setup(ctx);
        if (variant.MissShader.empty()) {
            IG_LOG(L_ERROR) << "Constructed empty miss shader." << std::endl;
            return false;
        }

        // Generate Hit Shader
        for (size_t j = 0; j < ctx.Environment.Materials.size(); ++j) {
            IG_LOG(L_DEBUG) << "Generating hit shader " << j << " for variant " << i << std::endl;
            std::string shader = HitShader::setup(j, ctx);
            if (shader.empty()) {
                IG_LOG(L_ERROR) << "Constructed empty hit shader for material " << j << "." << std::endl;
                return false;
            }
            variant.HitShaders.push_back(shader);
        }

        // Generate Advanced Shadow Shaders if requested
        if (info.ShadowHandlingMode != ShadowHandlingMode::Simple) {
            const size_t max_materials = info.ShadowHandlingMode == ShadowHandlingMode::Advanced ? 1 : ctx.Environment.Materials.size();
            for (size_t j = 0; j < max_materials; ++j) {
                IG_LOG(L_DEBUG) << "Generating advanced shadow hit shader " << j << " for variant " << i << std::endl;
                std::string shader = AdvancedShadowShader::setup(true, j, ctx);
                if (shader.empty()) {
                    IG_LOG(L_ERROR) << "Constructed empty advanced shadow hit shader for material " << j << "." << std::endl;
                    return false;
                }
                variant.AdvancedShadowHitShaders.push_back(shader);
            }
            for (size_t j = 0; j < max_materials; ++j) {
                IG_LOG(L_DEBUG) << "Generating advanced shadow miss shader " << j << " for variant " << i << std::endl;
                std::string shader = AdvancedShadowShader::setup(false, j, ctx);
                if (shader.empty()) {
                    IG_LOG(L_ERROR) << "Constructed empty advanced shadow miss shader for material " << j << "." << std::endl;
                    return false;
                }
                variant.AdvancedShadowMissShaders.push_back(shader);
            }
        }

        for (size_t j = 0; j < info.CallbackGenerators.size(); ++j) {
            if (info.CallbackGenerators.at(j) != nullptr) {
                IG_LOG(L_DEBUG) << "Generating callback shader " << j << " for variant " << i << std::endl;
                variant.CallbackShaders[j] = info.CallbackGenerators.at(j)(ctx);
            }
        }
    }

    result.Database.SceneRadius = ctx.Environment.SceneDiameter / 2.0f;
    result.Database.SceneBBox   = ctx.Environment.SceneBBox;
    result.TechniqueInfo        = ctx.TechniqueInfo;

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