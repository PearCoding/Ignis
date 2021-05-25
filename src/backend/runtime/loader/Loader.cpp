#include "Loader.h"
#include "LoaderBSDF.h"
#include "LoaderEntity.h"
#include "LoaderLight.h"
#include "LoaderShape.h"
#include "LoaderTexture.h"
#include "Logger.h"
#include "driver/Configuration.h"

#include <chrono>

namespace IG {
bool Loader::load(const LoaderOptions& opts, LoaderResult& result)
{
	const auto start1 = std::chrono::high_resolution_clock::now();

	LoaderContext ctx;
	ctx.FilePath	  = opts.FilePath;
	ctx.Target		  = configurationToTarget(opts.Configuration);
	ctx.EnablePadding = doesTargetRequirePadding(ctx.Target);
	ctx.Scene		  = opts.Scene;

	if (!LoaderTexture::load(ctx, result))
		return false;

	if (!LoaderShape::load(ctx, result))
		return false;

	if (!LoaderBSDF::load(ctx, result))
		return false;

	if (!LoaderLight::setup_area(ctx))
		return false;

	if (!LoaderEntity::load(ctx, result))
		return false;

	if (!LoaderLight::load(ctx, result))
		return false;

	result.Database.SceneRadius = ctx.Environment.SceneDiameter / 2.0f;

	IG_LOG(L_DEBUG) << "Loading scene took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start1).count() / 1000.0f << " seconds" << std::endl;

	return true;
}
} // namespace IG