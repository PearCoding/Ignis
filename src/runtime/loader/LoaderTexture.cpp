#include "LoaderTexture.h"
#include "Image.h"
#include "Loader.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "ShadingTree.h"
#include "Transpiler.h"

namespace IG {
static void tex_image(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    if (!tree.beginClosure(name))
        return;

    const std::filesystem::path filename = tree.context().handlePath(tex.property("filename").getString(), tex);
    const std::string filter_type        = tex.property("filter_type").getString("bicubic");
    const Transformf transform           = tex.property("transform").getTransform();
    const bool force_unpacked            = tex.property("force_unpacked").getBool(false); // Force the use of unpacked (float) images
    const bool linear                    = tex.property("linear").getBool(false);         // Hint that the image is already in linear. Only important if image type is not EXR or HDR, as they are always given in linear

    size_t res_id = tree.context().registerExternalResource(filename);

    std::string filter = "make_bicubic_filter()";
    if (filter_type == "bilinear")
        filter = "make_bilinear_filter()";
    else if (filter_type == "nearest")
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

    const std::string tex_id = tree.getClosureID(name);

    // Anonymize lookup by using the local registry
    tree.context().LocalRegistry.IntParameters["img_" + tex_id] = (int32)res_id;

    const size_t channel_count = Image::extractChannelCount(filename);

    stream << "  let img_" << tex_id << "_res_id = device.get_local_parameter_i32(\"img_" << tex_id << "\", 0);" << std::endl;
    if (!force_unpacked && Image::isPacked(filename))
        stream << "  let img_" << tex_id << " = device.load_packed_image_by_id(img_" << tex_id << "_res_id, " << channel_count << ", " << (linear ? "true" : "false") << ");" << std::endl;
    else
        stream << "  let img_" << tex_id << " = device.load_image_by_id(img_" << tex_id << "_res_id, " << channel_count << ");" << std::endl;

    stream << "  let tex_" << tex_id << " : Texture = make_image_texture("
           << wrap << ", "
           << filter << ", "
           << "img_" << tex_id << ", "
           << LoaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    tree.endClosure();
}

static void tex_checkerboard(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    if (!tree.beginClosure(name))
        return;

    tree.addColor("color0", tex, Vector3f::Zero(), true);
    tree.addColor("color1", tex, Vector3f::Ones(), true);
    tree.addNumber("scale_x", tex, 2.0f, true);
    tree.addNumber("scale_y", tex, 2.0f, true);

    const Transformf transform = tex.property("transform").getTransform();

    const std::string tex_id = tree.getClosureID(name);
    stream << tree.pullHeader()
           << "  let tex_" << tex_id << " : Texture = make_checkerboard_texture("
           << "make_vec2(" << tree.getInline("scale_x") << ", " << tree.getInline("scale_y") << "), "
           << tree.getInline("color0") << ", "
           << tree.getInline("color1") << ", "
           << LoaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    tree.endClosure();
}

static void tex_brick(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    if (!tree.beginClosure(name))
        return;

    tree.addColor("color0", tex, Vector3f::Zero(), true);
    tree.addColor("color1", tex, Vector3f::Ones(), true);
    tree.addNumber("scale_x", tex, 3.0f, true);
    tree.addNumber("scale_y", tex, 6.0f, true);
    tree.addNumber("gap_x", tex, 0.05f, true);
    tree.addNumber("gap_y", tex, 0.1f, true);

    const Transformf transform = tex.property("transform").getTransform();

    const std::string tex_id = tree.getClosureID(name);
    stream << tree.pullHeader()
           << "  let tex_" << tex_id << " : Texture = make_brick_texture("
           << tree.getInline("color0") << ", "
           << tree.getInline("color1") << ", "
           << "make_vec2(" << tree.getInline("scale_x") << ", " << tree.getInline("scale_y") << "), "
           << "make_vec2(" << tree.getInline("gap_x") << ", " << tree.getInline("gap_y") << "), "
           << LoaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    tree.endClosure();
}

static void tex_gen_noise(const std::string& func, std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    constexpr float DefaultSeed = 36326639.0f;

    if (!tree.beginClosure(name))
        return;

    tree.addColor("color", tex, Vector3f::Ones(), true);
    tree.addNumber("seed", tex, DefaultSeed, true);
    tree.addNumber("scale_x", tex, 10.0f, true);
    tree.addNumber("scale_y", tex, 10.0f, true);
    const Transformf transform = tex.property("transform").getTransform();

    std::string afunc = tex.property("colored").getBool() ? "c" + func : func;

    const std::string tex_id = tree.getClosureID(name);
    stream << tree.pullHeader()
           << "  let tex_" << tex_id << " : Texture = make_" << afunc << "_texture("
           << "make_vec2(" << tree.getInline("scale_x") << ", " << tree.getInline("scale_y") << "), "
           << tree.getInline("color") << ", "
           << tree.getInline("seed") << ", "
           << LoaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    tree.endClosure();
}
static void tex_noise(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    return tex_gen_noise("noise", stream, name, tex, tree);
}
static void tex_cellnoise(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    return tex_gen_noise("cellnoise", stream, name, tex, tree);
}
static void tex_pnoise(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    return tex_gen_noise("pnoise", stream, name, tex, tree);
}
static void tex_perlin(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    return tex_gen_noise("perlin", stream, name, tex, tree);
}
static void tex_voronoi(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    // TODO: More customization
    return tex_gen_noise("voronoi", stream, name, tex, tree);
}
static void tex_fbm(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    // TODO: More customization
    return tex_gen_noise("fbm", stream, name, tex, tree);
}

inline static bool startsWith(std::string_view str, std::string_view prefix)
{
    return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

static void tex_expr(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    if (!tree.beginClosure(name))
        return;

    std::string expr = tex.property("expr").getString();
    if (expr.empty()) {
        IG_LOG(L_ERROR) << "Texture '" << name << "' requires an expression" << std::endl;
        expr = "vec4(1, 0, 1, 1)"; // ~ Pink -> Mark as error
    }

    // Register on the shading tree first
    for (const auto& pair : tex.properties()) {
        if (startsWith(pair.first, "num_"))
            tree.addNumber(pair.first, tex, 0.0f, true);
        else if (startsWith(pair.first, "color_"))
            tree.addColor(pair.first, tex, Vector3f::Ones(), true);
    }

    // Register available variables to transpiler as well
    Transpiler transpiler(tree);
    for (const auto& pair : tex.properties()) {
        if (startsWith(pair.first, "num_"))
            transpiler.registerCustomVariableNumber(pair.first.substr(4));
        else if (startsWith(pair.first, "color_"))
            transpiler.registerCustomVariableColor(pair.first.substr(6));
        else if (startsWith(pair.first, "vec_"))
            transpiler.registerCustomVariableVector(pair.first.substr(4));
        else if (startsWith(pair.first, "bool_"))
            transpiler.registerCustomVariableBool(pair.first.substr(5));
    }

    // Transpile
    auto res    = transpiler.transpile(expr);
    bool failed = !res.has_value();
    if (failed) {
        // Mark as failed output
        res = Transpiler::Result{ "color_builtins::pink", {}, {}, false };
    }

    // Patch output to color
    std::string output;
    if (res.value().ScalarOutput)
        output = "make_gray_color(" + res.value().Expr + ")";
    else
        output = res.value().Expr;

    // Make sure all texture is loaded
    for (const auto& used_tex : res.value().Textures)
        tree.registerTextureUsage(used_tex);

    // Pull texture usage
    const std::string tex_id = tree.getClosureID(name);
    stream << tree.pullHeader()
           << "  let tex_" << tex_id << " : Texture = @|ctx| -> Color {" << std::endl;

    if (!failed) {
        // Inline custom variables
        for (const auto& pair : tex.properties()) {
            if (startsWith(pair.first, "num_")) {
                const std::string var_id = tree.getClosureID(pair.first.substr(4));
                stream << "    let var_tex_" << var_id << " = " << tree.getInline(pair.first) << ";" << std::endl;
            } else if (startsWith(pair.first, "color_")) {
                const std::string var_id = tree.getClosureID(pair.first.substr(6));
                stream << "    let var_tex_" << var_id << " = " << tree.getInline(pair.first) << ";" << std::endl;
            } else if (startsWith(pair.first, "vec_")) {
                const std::string var_id = tree.getClosureID(pair.first.substr(4));
                stream << "    let var_tex_" << var_id << " = " << LoaderUtils::inlineVector(pair.second.getVector3()) << ";" << std::endl;
            } else if (startsWith(pair.first, "bool_")) {
                const std::string var_id = tree.getClosureID(pair.first.substr(5));
                stream << "    let var_tex_" << var_id << " = " << (pair.second.getBool() ? "true" : "false") << ";" << std::endl;
            }
        }
    }

    // End output
    stream << "    " << output << " };" << std::endl;

    tree.endClosure();
}

static void tex_transform(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree)
{
    if (!tree.beginClosure(name))
        return;

    tree.addTexture("texture", tex, true);

    const Transformf transform = tex.property("transform").getTransform();

    const std::string tex_id = tree.getClosureID(name);
    stream << tree.pullHeader()
           << "  let tex_" << tex_id << " : Texture = make_transform_texture("
           << tree.getInline("texture") << ", "
           << LoaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    tree.endClosure();
}

using TextureLoader = void (*)(std::ostream& stream, const std::string& name, const Parser::Object& tex, ShadingTree& tree);
static const struct {
    const char* Name;
    TextureLoader Loader;
} _generators[] = {
    { "image", tex_image },
    { "bitmap", tex_image },
    { "brick", tex_brick },
    { "checkerboard", tex_checkerboard },
    { "noise", tex_noise },
    { "cellnoise", tex_cellnoise },
    { "pnoise", tex_pnoise },
    { "perlin", tex_perlin },
    { "voronoi", tex_voronoi },
    { "fbm", tex_fbm },
    { "expr", tex_expr },
    { "transform", tex_transform },
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
    stream << "  let tex_" << tree.getClosureID(name) << " : Texture = make_invalid_texture();" << std::endl;
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
