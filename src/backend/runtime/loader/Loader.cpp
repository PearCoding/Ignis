#include "Loader.h"
#include "HitShader.h"
#include "LoaderBSDF.h"
#include "LoaderEntity.h"
#include "LoaderLight.h"
#include "LoaderShape.h"
#include "LoaderTexture.h"
#include "Logger.h"
#include "MissShader.h"
#include "RayGenerationShader.h"

#include <chrono>

namespace IG {
bool Loader::load(const LoaderOptions& opts, LoaderResult& result)
{
	const auto start1 = std::chrono::high_resolution_clock::now();

	LoaderContext ctx;
	ctx.FilePath	  = opts.FilePath;
	ctx.Target		  = opts.Target;
	ctx.EnablePadding = doesTargetRequirePadding(ctx.Target);
	ctx.Scene		  = opts.Scene;
	ctx.CameraType	  = opts.CameraType;
	ctx.TechniqueType = opts.TechniqueType;

	// Load content
	if (!LoaderShape::load(ctx, result))
		return false;

	if (!LoaderEntity::load(ctx, result))
		return false;

	// Generate Ray Generation Shader
	result.RayGenerationShader = RayGenerationShader::setup(ctx, result);
	if (result.RayGenerationShader.empty())
		return false;

	// Generate Miss Shader
	result.MissShader = MissShader::setup(ctx, result);
	if (result.MissShader.empty())
		return false;

	// Generate Hit Shader
	for (size_t i = 0; i < result.Database.EntityTable.entryCount(); ++i) {
		std::string shader = HitShader::setup(i, ctx, result);
		if (shader.empty())
			return false;
		result.HitShaders.push_back(shader);
	}

	result.Database.SceneRadius = ctx.Environment.SceneDiameter / 2.0f;

	IG_LOG(L_DEBUG) << "Loading scene took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start1).count() / 1000.0f << " seconds" << std::endl;

	return true;
}
} // namespace IG