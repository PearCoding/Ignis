#include "ShadingTree.h"
#include "Loader.h"
#include "LoaderTexture.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "Transpiler.h"

namespace IG {
ShadingTree::ShadingTree(LoaderContext& ctx)
    : mContext(ctx)
{
    beginClosure();
}

void ShadingTree::addNumber(const std::string& name, const Parser::Object& obj, float def, bool hasDef, InlineMode mode)
{
    if (hasParameter(name))
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << name << "'" << std::endl;

    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case Parser::PT_NONE:
        if (!hasDef)
            return;
        inline_str = std::to_string(def);
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        inline_str = std::to_string(prop.getNumber());
        break;
    case Parser::PT_VECTOR3:
        IG_LOG(L_WARNING) << "Parameter '" << name << "' expects a number but a color was given. Using average instead" << std::endl;
        inline_str = std::to_string(prop.getVector3().mean());
        break;
    case Parser::PT_STRING:
        inline_str = handleTexture(prop.getString(), mode == IM_Bare ? "tex_coords" : "surf.tex_coords", false);
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addColor(const std::string& name, const Parser::Object& obj, const Vector3f& def, bool hasDef, InlineMode mode)
{
    if (hasParameter(name))
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << name << "'" << std::endl;

    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case Parser::PT_NONE:
        if (!hasDef)
            return;
        inline_str = "make_color(" + std::to_string(def.x()) + ", " + std::to_string(def.y()) + ", " + std::to_string(def.z()) + ", 1)";
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        inline_str = "make_gray_color(" + std::to_string(prop.getNumber()) + ")";
        break;
    case Parser::PT_VECTOR3: {
        Vector3f color = prop.getVector3();
        inline_str     = "make_color(" + std::to_string(color.x()) + ", " + std::to_string(color.y()) + ", " + std::to_string(color.z()) + ", 1)";
    } break;
    case Parser::PT_STRING:
        inline_str = handleTexture(prop.getString(), mode == IM_Bare ? "tex_coords" : "surf.tex_coords", true);
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

// Only use this if no basic color information suffices
void ShadingTree::addTexture(const std::string& name, const Parser::Object& obj, bool hasDef)
{
    if (hasParameter(name))
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << name << "'" << std::endl;

    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case Parser::PT_NONE:
        if (!hasDef)
            return;
        inline_str = "make_black_texture()";
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        IG_LOG(L_WARNING) << "Parameter '" << name << "' expects texture but only a number was given" << std::endl;
        inline_str = "make_constant_texture(make_gray_color(" + std::to_string(prop.getNumber()) + "))";
        break;
    case Parser::PT_VECTOR3: {
        Vector3f color = prop.getVector3();
        inline_str     = "make_constant_texture(make_color(" + std::to_string(color.x()) + ", " + std::to_string(color.y()) + ", " + std::to_string(color.z()) + ", 1))";
    } break;
    case Parser::PT_STRING: {
        std::string tex_func = handleTexture(prop.getString(), "uv", true);
        inline_str           = "@|uv:Vec2|->Color{" + tex_func + "}";
    } break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::beginClosure()
{
    mClosures.emplace_back();
}

void ShadingTree::endClosure()
{
    IG_ASSERT(mClosures.size() > 1, "Invalid closure end");
    mClosures.pop_back();
}

std::string ShadingTree::pullHeader()
{
    std::stringstream stream;
    for (const auto& lines : mHeaderLines)
        stream << lines;
    mHeaderLines.clear();
    return stream.str();
}

std::string ShadingTree::getInline(const std::string& name) const
{
    if (hasParameter(name))
        return currentClosure().Parameters.at(name);
    IG_LOG(L_ERROR) << "Trying to access unknown parameter '" << name << "'" << std::endl;
    return "";
}

void ShadingTree::registerTexture(const std::string& name)
{
    if (mLoadedTextures.count(name) == 0) {
        const auto tex = mContext.Scene.texture(name);
        if (!tex) {
            IG_LOG(L_ERROR) << "Unknown texture '" << name << "'" << std::endl;
            mHeaderLines.push_back("tex_" + ShaderUtils::escapeIdentifier(name) + " = make_invalid_texture();");
        } else {
            mHeaderLines.push_back(LoaderTexture::generate(name, *tex, *this));
        }
        mLoadedTextures.insert(name);
    }
}

std::string ShadingTree::handleTexture(const std::string& expr, const std::string& uv_access, bool needColor)
{
    Transpiler transpiler(mContext);
    auto res = transpiler.transpile(expr, uv_access);

    if (!res.has_value()) {
        if (needColor)
            return "color_builtins::pink/*Error*/";
        else
            return "0:f32/*Error*/";
    } else {
        for (const auto& tex : res.value().Textures)
            registerTexture(tex);

        if (needColor) {
            if (res.value().ScalarOutput)
                return "make_gray_color(" + res.value().Expr + ")";
            else
                return res.value().Expr;
        } else {
            if (res.value().ScalarOutput) {
                return res.value().Expr;
            } else {
                IG_LOG(L_WARNING) << "Expected expression '" << expr << "' to return a number but a color was returned instead. Using average instead" << std::endl;
                return "color_average(" + res.value().Expr + ")";
            }
        }
    }
}
} // namespace IG