#include "Loader.h"
#include "Parser.h"
#include "loader/LoaderShape.h"

namespace IG {
bool Loader::load(const std::filesystem::path& filepath, const LoaderOptions& opts, LoaderResult& result)
{
	LoaderContext ctx;
	ctx.EnablePadding = doesTargetRequirePadding(opts.Target);
	ctx.FilePath	  = filepath;
	ctx.Target		  = opts.Target;

	// Parse scene file
	bool ok = false;
	Parser::SceneParser parser;
	ctx.Scene = parser.loadFromFile(filepath.generic_u8string(), ok);
	if (!ok)
		return false;

	if (!LoaderShape::load(ctx, result))
		return false;

	return true;
}
} // namespace IG