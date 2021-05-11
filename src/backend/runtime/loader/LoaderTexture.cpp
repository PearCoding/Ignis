#include "LoaderTexture.h"
#include "Logger.h"

namespace IG {
/* static void tex_image(const std::shared_ptr<Parser::Object>& tex, const LoaderContext& ctx, std::ostream& os)
{
	const std::string filename	  = ctx.handlePath(tex->property("filename").getString());
	const std::string filter_type = tex->property("filter_type").getString("bilinear");
	const std::string wrap_mode	  = tex->property("wrap_mode").getString("repeat");

	std::string filter_m;
	if (filter_type == "nearest")
		filter_m = "make_nearest_filter()";
	else
		filter_m = "make_bilinear_filter()";

	std::string wrap_m;
	if (wrap_mode == "mirror")
		wrap_m = "make_mirror_border()";
	else if (wrap_mode == "clamp")
		wrap_m = "make_clamp_border()";
	else
		wrap_m = "make_repeat_border()";

	os << "make_texture(math, " << wrap_m << ", " << filter_m << ", device.load_image(\"" << filename << "\"))";
}

static void tex_checkerboard(const std::shared_ptr<Parser::Object>& tex, const LoaderContext& ctx, std::ostream& os)
{
	const float scale_x = tex->property("scale_x").getNumber(1.0f);
	const float scale_y = tex->property("scale_y").getNumber(1.0f);
	bool _ignore;
	const auto color0 = ctx.extractMaterialPropertyColorLight(tex, "color0", 0.0f, _ignore);
	const auto color1 = ctx.extractMaterialPropertyColorLight(tex, "color1", 1.0f, _ignore);

	os << "make_checkerboard_texture(make_vec2(" << scale_x << ", " << scale_y << "), " << color0 << ", " << color1 << ")";
}

using TextureLoader = void (*)(const std::shared_ptr<Parser::Object>& tex, const LoaderContext& ctx, std::ostream& os);
static struct {
	const char* Name;
	TextureLoader Loader;
} _generators[] = {
	{ "image", tex_image },
	{ "bitmap", tex_image },
	{ "checkerboard", tex_checkerboard },
	{ "", nullptr }
}; */

bool LoaderTexture::load(const std::shared_ptr<Parser::Object>& tex, const LoaderContext& ctx, LoaderResult& res)
{
	return true;
}
} // namespace IG