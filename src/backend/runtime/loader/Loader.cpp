#include "Loader.h"
#include "LoaderBSDF.h"
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
    const auto start1 = std::chrono::high_resolution_clock::now();

    LoaderContext ctx;
    ctx.FilePath            = opts.FilePath;
    ctx.Target              = opts.Target;
    ctx.EnablePadding       = doesTargetRequirePadding(ctx.Target);
    ctx.Scene               = opts.Scene;
    ctx.CameraType          = opts.CameraType;
    ctx.TechniqueType       = opts.TechniqueType;
    ctx.SamplesPerIteration = opts.SamplesPerIteration;

    // Load content
    if (!LoaderShape::load(ctx, result))
        return false;

    if (!LoaderEntity::load(ctx, result))
        return false;

    ctx.Database      = &result.Database;
    ctx.TechniqueInfo = LoaderTechnique::getInfo(ctx);

    LoaderLight::setupAreaLights(ctx);

    result.TechniqueVariants.resize(ctx.TechniqueInfo.VariantCount);
    for (uint32 i = 0; i < ctx.TechniqueInfo.VariantCount; ++i) {
        auto& variant               = result.TechniqueVariants[i];
        ctx.CurrentTechniqueVariant = i;

        // Generate Ray Generation Shader
        if (ctx.TechniqueInfo.OverrideCameraGenerator[ctx.CurrentTechniqueVariant])
            variant.RayGenerationShader = ctx.TechniqueInfo.OverrideCameraGenerator[ctx.CurrentTechniqueVariant](ctx);
        else
            variant.RayGenerationShader = RayGenerationShader::setup(ctx);
        if (variant.RayGenerationShader.empty())
            return false;

        // Generate Miss Shader
        variant.MissShader = MissShader::setup(ctx);
        if (variant.MissShader.empty())
            return false;

        // Generate Hit Shader
        for (size_t i = 0; i < result.Database.EntityTable.entryCount(); ++i) {
            std::string shader = HitShader::setup(i, ctx);
            if (shader.empty())
                return false;
            variant.HitShaders.push_back(shader);
        }

        // Generate Advanced Shadow Shaders if requested
        if (ctx.TechniqueInfo.UseAdvancedShadowHandling[ctx.CurrentTechniqueVariant]) {
            variant.AdvancedShadowHitShader  = AdvancedShadowShader::setup(true, ctx);
            variant.AdvancedShadowMissShader = AdvancedShadowShader::setup(false, ctx);
        }
    }

    result.Database.SceneRadius = ctx.Environment.SceneDiameter / 2.0f;
    result.Database.SceneBBox   = ctx.Environment.SceneBBox;
    result.AOVs                 = ctx.TechniqueInfo.EnabledAOVs;

    IG_LOG(L_DEBUG) << "Loading scene took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start1).count() / 1000.0f << " seconds" << std::endl;

    return true;
}
} // namespace IG