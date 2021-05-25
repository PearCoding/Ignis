#include "LoaderTexture.h"
#include "Loader.h"
#include "Logger.h"
#include "serialization/VectorSerializer.h"

namespace IG {
enum TextureType {
	TEX_IMAGE		 = 0x00,
	TEX_CHECKERBOARD = 0x10,
	TEX_INVALID		 = 0xFF
};

enum TextureImageFilter {
	TIF_NEAREST	 = 0x0,
	TIF_BILINEAR = 0x1
};

enum TextureImageWrap {
	TIW_REPEAT = 0x0,
	TIW_MIRROR = 0x1,
	TIW_CLAMP
};

static void tex_image(const std::string& name, const std::shared_ptr<Parser::Object>& tex, LoaderContext& ctx, LoaderResult& result)
{
	const std::string filename	  = ctx.handlePath(tex->property("filename").getString());
	const std::string filter_type = tex->property("filter_type").getString("bilinear");
	const std::string wrap_mode	  = tex->property("wrap_mode").getString("repeat");

	bool ok					= false;
	uint32 bufferID			= ctx.loadImage(filename, result.Database, ok);
	ctx.TextureBuffer[name] = bufferID;

	if (ok) {
		uint32 filter = TIF_BILINEAR;
		if (filter_type == "nearest")
			filter = TIF_NEAREST;

		uint32 wrap = TIW_REPEAT;
		if (wrap_mode == "mirror")
			wrap = TIW_MIRROR;
		else if (wrap_mode == "clamp")
			wrap = TIW_CLAMP;

		uint32 type_id = 0;
		if (wrap == TIW_CLAMP && filter == TIF_NEAREST)
			type_id = 0;
		else if (wrap == TIW_CLAMP && filter == TIF_BILINEAR)
			type_id = 1;
		else if (wrap == TIW_REPEAT && filter == TIF_NEAREST)
			type_id = 2;
		else if (wrap == TIW_REPEAT && filter == TIF_BILINEAR)
			type_id = 3;
		else if (wrap == TIW_MIRROR && filter == TIF_NEAREST)
			type_id = 4;
		else if (wrap == TIW_MIRROR && filter == TIF_BILINEAR)
			type_id = 5;

		auto& data = result.Database.TextureTable.addLookup(type_id, 0, DefaultAlignment);
		VectorSerializer serializer(data, false);
		serializer.write(bufferID);
		serializer.write(filter);
		serializer.write(wrap);
	} else {
		result.Database.TextureTable.addLookup(TEX_INVALID, 0, DefaultAlignment);
	}
}

static void tex_checkerboard(const std::string&, const std::shared_ptr<Parser::Object>& tex, LoaderContext& ctx, LoaderResult& result)
{
	const auto color0	= ctx.extractColor(tex, "color0", Vector3f::Zero());
	const auto color1	= ctx.extractColor(tex, "color1", Vector3f::Ones());
	const float scale_x = tex->property("scale_x").getNumber(1.0f);
	const float scale_y = tex->property("scale_y").getNumber(1.0f);

	auto& data = result.Database.TextureTable.addLookup(TEX_CHECKERBOARD, 0, DefaultAlignment);
	VectorSerializer serializer(data, false);
	serializer.write(color0);
	serializer.write(scale_x);
	serializer.write(color1);
	serializer.write(scale_y);
}

using TextureLoader = void (*)(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, LoaderContext& ctx, LoaderResult& result);
static struct {
	const char* Name;
	TextureLoader Loader;
} _generators[] = {
	{ "image", tex_image },
	{ "bitmap", tex_image },
	{ "checkerboard", tex_checkerboard },
	{ "", nullptr }
};

bool LoaderTexture::load(LoaderContext& ctx, LoaderResult& result)
{
	const auto start1 = std::chrono::high_resolution_clock::now();

	// TODO: Change to BSDF like bookkeeping of ignored elements
	size_t counter = 0;
	for (const auto& pair : ctx.Scene.textures()) {
		ctx.Environment.TextureID[pair.first] = counter++;
	}

	for (const auto& pair : ctx.Scene.textures()) {
		const auto tex = pair.second;

		bool found = false;
		for (size_t i = 0; _generators[i].Loader; ++i) {
			if (_generators[i].Name == tex->pluginType()) {
				_generators[i].Loader(pair.first, tex, ctx, result);
				found = true;
				break;
			}
		}

		if (!found) {
			IG_LOG(L_ERROR) << "Texture '" << pair.first << "' has unknown type '" << tex->pluginType() << "'" << std::endl;
			result.Database.TextureTable.addLookup(TEX_INVALID, 0, DefaultAlignment);
		}
	}

	IG_LOG(L_DEBUG) << "Storing textures took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start1).count() / 1000.0f << " seconds" << std::endl;

	return true;
}
} // namespace IG