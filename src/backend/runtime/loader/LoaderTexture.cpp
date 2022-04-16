#include "LoaderTexture.h"
#include "Loader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "ShadingTree.h"

namespace IG {
static void tex_image(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    tree.beginClosure();

    const std::string filename    = tree.context().handlePath(tex.property("filename").getString(), tex).generic_u8string();
    const std::string filter_type = tex.property("filter_type").getString("bilinear");
    const Transformf transform    = tex.property("transform").getTransform();

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
           << ShaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    tree.endClosure();
}

static void tex_checkerboard(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    tree.beginClosure();

    tree.addColor("color0", tex, Vector3f::Zero());
    tree.addColor("color1", tex, Vector3f::Ones());
    tree.addNumber("scale_x", tex, 1.0f);
    tree.addNumber("scale_y", tex, 1.0f);

    const Transformf transform = tex.property("transform").getTransform();

    stream << tree.pullHeader()
           << "  let tex_" << ShaderUtils::escapeIdentifier(name) << " : Texture = make_checkerboard_texture("
           << "make_vec2(" << tree.getInline("scale_x") << ", " << tree.getInline("scale_y") << "), "
           << tree.getInline("color0") << ", "
           << tree.getInline("color1") << ", "
           << ShaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    tree.endClosure();
}

using TextureLoader = void (*)(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree);
static const struct {
    const char* Name;
    TextureLoader Loader;
} _generators[] = {
    { "image", tex_image },
    { "bitmap", tex_image },
    { "checkerboard", tex_checkerboard },
    { "", nullptr }
};

std::string LoaderTexture::generate(const std::string& name, const Parser::Object& obj, ShadingTree& tree)
{
    for (size_t i = 0; _generators[i].Loader; ++i) {
        if (_generators[i].Name == obj.pluginType()) {
            std::stringstream stream;
            _generators[i].Loader(stream, name, obj, tree);
            return stream.str();
        }
    }

    IG_LOG(L_ERROR) << "No texture type '" << obj.pluginType() << "' available" << std::endl;

    std::stringstream stream;
    stream << "  let tex_" << ShaderUtils::escapeIdentifier(name) << " : Texture = make_invalid_texture();" << std::endl;
    return stream.str();
}

std::filesystem::path LoaderTexture::getFilename(const Parser::Object& obj, const LoaderContext& ctx)
{
    for (size_t i = 0; _generators[i].Loader; ++i) {
        if (_generators[i].Name == obj.pluginType()) {
            if (_generators[i].Loader == tex_image) {
                return ctx.handlePath(obj.property("filename").getString(), obj);
            } else {
                return {};
            }
        }
    }

    IG_LOG(L_ERROR) << "No texture type '" << obj.pluginType() << "' available" << std::endl;
    return {};
}
} // namespace IG
