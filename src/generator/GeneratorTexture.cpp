#include "GeneratorTexture.h"
#include "Logger.h"

namespace IG {
static void tex_image(const std::shared_ptr<Loader::Object>& tex, const GeneratorContext& ctx, std::ostream& os)
{
	const std::string filename = ctx.handlePath(tex->property("filename").getString());
	os << "make_texture(math, make_repeat_border(), make_bilinear_filter(), device.load_image(\"" << filename << "\"))";
}

static void tex_checkerboard(const std::shared_ptr<Loader::Object>& tex, const GeneratorContext& ctx, std::ostream& os)
{
	IG_LOG(L_WARNING) << "TODO texture '" << tex->pluginType() << "'" << std::endl;
	os << "make_black_texture()/* TODO */";
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