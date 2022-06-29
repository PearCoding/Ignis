#include "ShadingTree.h"
#include "Loader.h"
#include "LoaderTexture.h"
#include "LoaderUtils.h"
#include "Logger.h"

namespace IG {
ShadingTree::ShadingTree(LoaderContext& ctx)
    : mContext(ctx)
    , mTranspiler(*this)
{
    beginClosure();
}

void ShadingTree::signalError()
{
    mContext.signalError();
}

void ShadingTree::addNumber(const std::string& name, const Parser::Object& obj, float def, bool hasDef)
{
    if (hasParameter(name)) {
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << name << "'" << std::endl;
        signalError();
    }

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
        inline_str = handleTexture(name, prop.getString(), false);
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addColor(const std::string& name, const Parser::Object& obj, const Vector3f& def, bool hasDef)
{
    if (hasParameter(name)) {
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << name << "'" << std::endl;
        signalError();
    }

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
        inline_str = handleTexture(name, prop.getString(), true);
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addVector(const std::string& name, const Parser::Object& obj, const Vector3f& def, bool hasDef)
{
    if (hasParameter(name)) {
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << name << "'" << std::endl;
        signalError();
    }

    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case Parser::PT_NONE:
        if (!hasDef)
            return;
        inline_str = "make_vec3(" + std::to_string(def.x()) + ", " + std::to_string(def.y()) + ", " + std::to_string(def.z()) + ")";
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        inline_str = "vec3_expand(" + std::to_string(prop.getNumber()) + ")";
        break;
    case Parser::PT_VECTOR3: {
        Vector3f color = prop.getVector3();
        inline_str     = "make_vec3(" + std::to_string(color.x()) + ", " + std::to_string(color.y()) + ", " + std::to_string(color.z()) + ")";
    } break;
    case Parser::PT_STRING:
        inline_str = "color_to_vec3(" + handleTexture(name, prop.getString(), true) + ")";
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

// Only use this if no basic color information suffices
void ShadingTree::addTexture(const std::string& name, const Parser::Object& obj, bool hasDef)
{
    if (hasParameter(name)) {
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << name << "'" << std::endl;
        signalError();
    }

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
        inline_str = "make_constant_texture(make_gray_color(" + std::to_string(prop.getNumber()) + "))";
        break;
    case Parser::PT_VECTOR3: {
        Vector3f color = prop.getVector3();
        inline_str     = "make_constant_texture(make_color(" + std::to_string(color.x()) + ", " + std::to_string(color.y()) + ", " + std::to_string(color.z()) + ", 1))";
    } break;
    case Parser::PT_STRING: {
        std::string tex_func = handleTexture(name, prop.getString(), true);
        inline_str           = "@|ctx:ShadingContext|->Color{maybe_unused(ctx); " + tex_func + "}";
    } break;
    }

    currentClosure().Parameters[name] = inline_str;
}

bool ShadingTree::beginClosure(const std::string& texName)
{
    if (!texName.empty()) {
        for (const auto& closure : mClosures) {
            if (closure.TexName == texName) {
                IG_LOG(L_ERROR) << "Texture '" << texName << "' calls itself, resulting in a cycle!" << std::endl;
                signalError();
                return false;
            }
        }
    }
    mClosures.emplace_back(Closure{ texName, {} });
    return true;
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

std::string ShadingTree::getInline(const std::string& name)
{
    if (hasParameter(name))
        return currentClosure().Parameters.at(name);
    IG_LOG(L_ERROR) << "Trying to access unknown parameter '" << name << "'" << std::endl;
    signalError();
    return "";
}

bool ShadingTree::isPureTexture(const std::string& name)
{
    if (hasParameter(name))
        return mPureTextures.count(name) > 0;
    IG_LOG(L_ERROR) << "Trying to access unknown parameter '" << name << "'" << std::endl;
    signalError();
    return "";
}

void ShadingTree::registerTextureUsage(const std::string& name)
{
    if (mLoadedTextures.count(name) == 0) {
        const auto tex = mContext.Scene.texture(name);
        if (!tex) {
            IG_LOG(L_ERROR) << "Unknown texture '" << name << "'" << std::endl;
            mHeaderLines.push_back("tex_" + generateUniqueID(name) + " = make_invalid_texture();");
        } else {
            const std::string res = LoaderTexture::generate(name, *tex, *this);
            if (res.empty()) // Due to some error this might happen
                return;
            mHeaderLines.push_back(res);
        }
        mLoadedTextures.insert(name);
    }
}

std::string ShadingTree::handleTexture(const std::string& prop_name, const std::string& expr, bool needColor)
{
    auto res = mTranspiler.transpile(expr);

    if (!res.has_value()) {
        if (needColor)
            return "color_builtins::pink/*Error*/";
        else
            return "0:f32/*Error*/";
    } else {
        for (const auto& tex : res.value().Textures)
            registerTextureUsage(tex);

        // Check if the call is just a pure texture (without uv or other modifications)
        if (res.value().Textures.size() == 1) {
            if (*res.value().Textures.begin() == expr)
                mPureTextures.insert(prop_name);
        }

        if (needColor) {
            if (res.value().ScalarOutput)
                return "make_gray_color(" + res.value().Expr + ")";
            else
                return res.value().Expr;
        } else {
            if (res.value().ScalarOutput) {
                return res.value().Expr;
            } else {
                IG_LOG(L_WARNING) << "Expected expression '" << expr << "' to return a number but a color was returned. Using average instead" << std::endl;
                return "color_average(" + res.value().Expr + ")";
            }
        }
    }
}

std::string ShadingTree::generateUniqueID(const std::string& name)
{
    auto it = mIDMap.find(name);
    if (it != mIDMap.end())
        return std::to_string(it->second);

    size_t id    = mIDMap.size();
    mIDMap[name] = id;
    return std::to_string(id);
}
} // namespace IG