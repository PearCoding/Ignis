#include "GeneratorTexture.h"
#include "Logger.h"

namespace IG {
static void tex_image(const std::shared_ptr<Loader::Object>& tex, const GeneratorContext& ctx, std::ostream& os)
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

static void tex_checkerboard(const std::shared_ptr<Loader::Object>& tex, const GeneratorContext& ctx, std::ostream& os)
{
	const float scale_x = tex->property("scale_x").getNumber(1.0f);
	const float scale_y = tex->property("scale_y").getNumber(1.0f);
	bool _ignore;
	const auto color0 = ctx.extractMaterialPropertyColorLight(tex, "color0", 0.0f, _ignore);
	const auto color1 = ctx.extractMaterialPropertyColorLight(tex, "color1", 1.0f, _ignore);

	os << "make_checkerboard_texture(make_vec2(" << scale_x << ", " << scale_y << "), " << color0 << ", " << color1 << ")";
}

static void tex_error(const std::string& msg, std::ostream& os)
{
	IG_LOG(L_ERROR) << msg << std::endl;
	os << "make_black_texture()/* ERROR */";
}

static void tex_unknown(const std::shared_ptr<Loader::Object>& tex, const GeneratorContext&, std::ostream& os)
{
	IG_LOG(L_WARNING) << "Unknown texture '" << tex->pluginType() << "'" << std::endl;
	os << "make_black_texture()/* Null */";
}

using TextureGenerator = void (*)(const std::shared_ptr<Loader::Object>& tex, const GeneratorContext& ctx, std::ostream& os);
static struct {
	const char* Name;
	TextureGenerator Generator;
} _generators[] = {
	{ "image", tex_image },
	{ "bitmap", tex_image },
	{ "checkerboard", tex_checkerboard },
	{ "", nullptr }
};

std::string GeneratorTexture::extract(const std::shared_ptr<Loader::Object>& tex, const GeneratorContext& ctx)
{
	std::stringstream sstream;

	if (!tex) {
		tex_error("No tex given", sstream);
	} else {
		for (size_t i = 0; _generators[i].Generator; ++i) {
			if (_generators[i].Name == tex->pluginType()) {
				_generators[i].Generator(tex, ctx, sstream);
				return sstream.str();
			}
		}
		tex_unknown(tex, ctx, sstream);
	}

	return sstream.str();
}
} // namespace IG