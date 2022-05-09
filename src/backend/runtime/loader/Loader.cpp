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
    ctx.FilePath            = opts.FilePath;
    ctx.Target              = opts.Target;
    ctx.EnablePadding       = doesTargetRequirePadding(ctx.Target);
    ctx.Scene               = opts.Scene;
    ctx.CameraType          = opts.CameraType;
    ctx.TechniqueType       = opts.TechniqueType;
    ctx.PixelSamplerType    = "independent"; // TODO
    ctx.SamplesPerIteration = opts.SamplesPerIteration;
    ctx.IsTracer            = opts.IsTracer;
    ctx.FilmWidth           = opts.FilmWidth;
    ctx.FilmHeight          = opts.FilmHeight;

    LoaderLight::setupAreaLights(ctx);

    // Load content
    if (!LoaderShape::load(ctx, result))
        return false;

    if (!LoaderEntity::load(ctx, result))
        return false;

    LoaderCamera::setupInitialOrientation(ctx, result);

    IG_LOG(L_DEBUG) << "Got " << ctx.Environment.Materials.size() << " unique materials" << std::endl;
    IG_LOG(L_DEBUG) << "Got " << ctx.Environment.AreaLightsMap.size() << " unique area lights" << std::endl;

    result.Database.MaterialCount = ctx.Environment.Materials.size();

    ctx.Database      = &result.Database;
    ctx.TechniqueInfo = LoaderTechnique::getInfo(ctx);

    if (ctx.TechniqueInfo.Variants.empty()) {
        IG_LOG(L_ERROR) << "Invalid technique with no variants" << std::endl;
        return false;
    }

    result.TechniqueVariants.resize(ctx.TechniqueInfo.Variants.size());
    for (size_t i = 0; i < ctx.TechniqueInfo.Variants.size(); ++i) {
        auto& variant               = result.TechniqueVariants[i];
        const auto& info            = ctx.TechniqueInfo.Variants[i];
        ctx.CurrentTechniqueVariant = i;
        ctx.SamplesPerIteration     = info.GetSPI(opts.SamplesPerIteration);

        // Generate Ray Generation Shader
        if (info.OverrideCameraGenerator)
            variant.RayGenerationShader = info.OverrideCameraGenerator(ctx);
        else
            variant.RayGenerationShader = RayGenerationShader::setup(ctx);
        if (variant.RayGenerationShader.empty()) {
            IG_LOG(L_ERROR) << "Constructed empty ray generation shader." << std::endl;
            return false;
        }

        // Generate Miss Shader
        variant.MissShader = MissShader::setup(ctx);
        if (variant.MissShader.empty()) {
            IG_LOG(L_ERROR) << "Constructed empty miss shader." << std::endl;
            return false;
        }

        // Generate Hit Shader
        for (size_t j = 0; j < ctx.Environment.Materials.size(); ++j) {
            std::string shader = HitShader::setup(j, ctx);
            if (shader.empty()) {
                IG_LOG(L_ERROR) << "Constructed empty hit shader for material " << j << "." << std::endl;
                return false;
            }
            variant.HitShaders.push_back(shader);
        }

        // Generate Advanced Shadow Shaders if requested
        if (info.UseAdvancedShadowHandling) {
            variant.AdvancedShadowHitShader  = AdvancedShadowShader::setup(true, ctx);
            variant.AdvancedShadowMissShader = AdvancedShadowShader::setup(false, ctx);
        }

        for (size_t j = 0; j < info.CallbackGenerators.size(); ++j) {
            if (info.CallbackGenerators.at(j) != nullptr)
                variant.CallbackShaders[j] = info.CallbackGenerators.at(j)(ctx);
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