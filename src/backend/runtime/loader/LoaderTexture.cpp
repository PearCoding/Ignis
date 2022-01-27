#include "LoaderTexture.h"
#include "Loader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "ShadingTree.h"

namespace IG {
static void tex_image(std::ostream& stream, const std::string& name, const Parser::Object& tex, const LoaderContext& ctx, ShadingTree& tree)
{
    IG_UNUSED(tree);

    const std::string filename    = ctx.handlePath(tex.property("filename").getString());
    const std::string filter_type = tex.property("filter_type").getString("bilinear");
    const bool flip_x             = tex.property("flip_x").getBool(false);
    const bool flip_y             = tex.property("flip_y").getBool(false);

    std::string filter = "make_bilinear_filter()";
    if (filter_type == "nearest")
        filter = "make_nearest_filter()";

    const auto getWrapMode = [](const std::string& str) {
        if (str == "mirror")
            return "make_mirror_border()";
        else if (str == "clamp")
            return "make_clamp_border()";
        else
            return "make_repeat_border()";
    };

    std::string wrap;
    if (tex.property("wrap_mode_u").isValid()) {
        std::string wrap_u = getWrapMode(tex.property("wrap_mode_u").getString("repeat"));
        std::string wrap_v = getWrapMode(tex.property("wrap_mode_v").getString("repeat"));
        if (wrap_u == wrap_v)
            wrap = wrap_u;
        else
            wrap = "make_split_border(" + wrap_u + ", " + wrap_v + ")";
    } else {
        wrap = getWrapMode(tex.property("wrap_mode").getString("repeat"));
    }

    stream << "  let img_" << ShaderUtils::escapeIdentifier(name) << " = device.load_image(\"" << filename << "\");" << std::endl
           << "  let tex_" << ShaderUtils::escapeIdentifier(name) << " : Texture = make_image_texture("
           << wrap << ", "
           << filter << ", "
           << "img_" << ShaderUtils::escapeIdentifier(name) << ", "
           << (flip_x ? "true" : "false") << ", "
           << (flip_y ? "true" : "false") << ");" << std::endl;
}

static void tex_checkerboard(std::ostream& stream, const std::string& name, const Parser::Object& tex, const LoaderContext& ctx, ShadingTree& tree)
{
    IG_UNUSED(tree);

    const auto color0   = ctx.extractColor(tex, "color0", Vector3f::Zero());
    const auto color1   = ctx.extractColor(tex, "color1", Vector3f::Ones());
    const float scale_x = tex.property("scale_x").getNumber(1.0f);
    const float scale_y = tex.property("scale_y").getNumber(1.0f);

    stream << "  let tex_" << ShaderUtils::escapeIdentifier(name) << " : Texture = make_checkerboard_texture("
           << "make_vec2(" << scale_x << ", " << scale_y << "), "
           << ShaderUtils::inlineColor(color0) << ", "
           << ShaderUtils::inlineColor(color1) << ");" << std::endl;
}

using TextureLoader = void (*)(std::ostream& stream, const std::string& name, const Parser::Object& tex, const LoaderContext& ctx, ShadingTree& tree);
static struct {
    const char* Name;
    TextureLoader Loader;
} _generators[] = {
    { "image", tex_image },
    { "bitmap", tex_image },
    { "checkerboard", tex_checkerboard },
    { "", nullptr }
};

std::string LoaderTexture::generate(const std::string& name, const Parser::Object& obj, const LoaderContext& ctx, ShadingTree& tree)
{
    for (size_t i = 0; _generators[i].Loader; ++i) {
        if (_generators[i].Name == obj.pluginType()) {
            std::stringstream stream;
            _generators[i].Loader(stream, name, obj, ctx, tree);
            return stream.str();
        }
    }

    IG_LOG(L_ERROR) << "No texture type '" << obj.pluginType() << "' available" << std::endl;

    std::stringstream stream;
    stream << "  let tex_" << ShaderUtils::escapeIdentifier(name) << " : Texture = make_invalid_texture();" << std::endl;
    return stream.str();
}

std::string LoaderTexture::getFilename(const Parser::Object& obj, const LoaderContext& ctx)
{
    for (size_t i = 0; _generators[i].Loader; ++i) {
        if (_generators[i].Name == obj.pluginType()) {
            if (_generators[i].Loader == tex_image) {
                return ctx.handlePath(obj.property("filename").getString());
            } else {
                return {};
            }
        }
    }

    IG_LOG(L_ERROR) << "No texture type '" << obj.pluginType() << "' available" << std::endl;
    return {};
}
} // namespace IG
