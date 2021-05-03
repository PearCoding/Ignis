#include "Loader.h"
#include "LoaderBSDF.h"
#include "LoaderEntity.h"
#include "LoaderLight.h"
#include "LoaderShape.h"
#include "driver/Configuration.h"

namespace IG {
bool Loader::load(const LoaderOptions& opts, LoaderResult& result)
{
	LoaderContext ctx;
	ctx.FilePath	  = opts.FilePath;
	ctx.Target		  = configurationToTarget(opts.Configuration);
	ctx.EnablePadding = doesTargetRequirePadding(ctx.Target);
	ctx.Scene		  = opts.Scene;

	if (!LoaderShape::load(ctx, result))
		return false;

	/*if (!LoaderBSDF::load(ctx, result))
		return false;

	if (!LoaderLight::load(ctx, result))
		return false;*/

	if (!LoaderEntity::load(ctx, result))
		return false;


	return true;
}
} // namespace IG