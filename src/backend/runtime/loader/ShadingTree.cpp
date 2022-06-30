#include "ShadingTree.h"
#include "Loader.h"
#include "LoaderTexture.h"
#include "LoaderUtils.h"
#include "Logger.h"

namespace IG {
ShadingTree::ShadingTree(LoaderContext& ctx)
    : mContext(ctx)
    , mTranspiler(*this)
    , mUseLocalRegistry(false)
{
    beginClosure("_root");
}

void ShadingTree::signalError()
{
    mContext.signalError();
}

void ShadingTree::addNumber(const std::string& name, const Parser::Object& obj, float def, bool hasDef, ShadingTreePropertyEmbedType embedType)
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
        inline_str = acquireNumber(name, def, embedType);
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        inline_str = acquireNumber(name, prop.getNumber(), embedType);
        break;
    case Parser::PT_VECTOR3:
        IG_LOG(L_WARNING) << "Parameter '" << name << "' expects a number but a color was given. Using average instead" << std::endl;
        inline_str = "color_luminance(" + acquireColor(name, prop.getVector3(), embedType) + ")";
        break;
    case Parser::PT_STRING:
        inline_str = handleTexture(name, prop.getString(), false);
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addColor(const std::string& name, const Parser::Object& obj, const Vector3f& def, bool hasDef, ShadingTreePropertyEmbedType embedType)
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
        inline_str = acquireColor(name, def, embedType);
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        inline_str = "make_gray_color(" + acquireNumber(name, prop.getNumber(), embedType) + ")";
        break;
    case Parser::PT_VECTOR3:
        inline_str = acquireColor(name, prop.getVector3(), embedType);
        break;
    case Parser::PT_STRING:
        inline_str = handleTexture(name, prop.getString(), true);
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addVector(const std::string& name, const Parser::Object& obj, const Vector3f& def, bool hasDef, ShadingTreePropertyEmbedType embedType)
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
        inline_str = acquireVector(name, def, embedType);
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        inline_str = "vec3_expand(" + acquireNumber(name, prop.getNumber(), embedType) + ")";
        break;
    case Parser::PT_VECTOR3:
        inline_str = acquireVector(name, prop.getVector3(), embedType);
        break;
    case Parser::PT_STRING:
        inline_str = "color_to_vec3(" + handleTexture(name, prop.getString(), true) + ")";
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

// Only use this if no basic color information suffices
void ShadingTree::addTexture(const std::string& name, const Parser::Object& obj, bool hasDef, ShadingTreePropertyEmbedType embedType)
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
        inline_str = "make_constant_texture(make_gray_color(" + acquireNumber(name, prop.getNumber(), embedType) + "))";
        break;
    case Parser::PT_VECTOR3:;
        inline_str = "make_constant_texture(" + acquireColor(name, prop.getVector3(), embedType) + ")";
        break;
    case Parser::PT_STRING: {
        std::string tex_func = handleTexture(name, prop.getString(), true);
        inline_str           = "@|ctx:ShadingContext|->Color{maybe_unused(ctx); " + tex_func + "}";
    } break;
    }

    currentClosure().Parameters[name] = inline_str;
}

bool ShadingTree::beginClosure(const std::string& name)
{
    for (const auto& closure : mClosures) {
        if (closure.Name == name) {
            IG_LOG(L_ERROR) << "Closure '" << name << "' calls itself, resulting in a cycle!" << std::endl;
            signalError();
            return false;
        }
    }

    mClosures.emplace_back(Closure{ name, generateUniqueID(name), {} });
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

bool ShadingTree::checkIfEmbed(ShadingTreePropertyEmbedType embedType) const
{
    switch (embedType) {
    case ShadingTreePropertyEmbedType::Structural:
        return true;
    case ShadingTreePropertyEmbedType::Dynamic:
        return false;
    default:
    case ShadingTreePropertyEmbedType::Default:
        return !(mUseLocalRegistry || context().ForceLocalRegistryUsageForShadingTrees);
    }
}

std::string ShadingTree::acquireNumber(const std::string& prop_name, float number, ShadingTreePropertyEmbedType embedType)
{
    if (checkIfEmbed(embedType)) {
        return std::to_string(number);
    } else {
        const std::string id                       = currentClosureID() + "_" + LoaderUtils::escapeIdentifier(prop_name);
        const size_t reg_id                        = mContext.LocalRegistry.FloatParameters.size();
        mContext.LocalRegistry.FloatParameters[id] = number;

        mHeaderLines.push_back("let var_num_" + id + " = device.get_local_parameter_f32(" + std::to_string(reg_id) + ");");
        return "var_num_" + id;
    }
}

std::string ShadingTree::acquireColor(const std::string& prop_name, const Vector3f& color, ShadingTreePropertyEmbedType embedType)
{
    if (checkIfEmbed(embedType)) {
        return "make_color(" + std::to_string(color.x()) + ", " + std::to_string(color.y()) + ", " + std::to_string(color.z()) + ", 1)";
    } else {
        const std::string id                       = currentClosureID() + "_" + LoaderUtils::escapeIdentifier(prop_name);
        const size_t reg_id                        = mContext.LocalRegistry.ColorParameters.size();
        mContext.LocalRegistry.ColorParameters[id] = Vector4f(color.x(), color.y(), color.z(), 1);

        mHeaderLines.push_back("let var_color_" + id + " = device.get_local_parameter_f32(" + std::to_string(reg_id) + ");");
        return "var_color_" + id;
    }
}

std::string ShadingTree::acquireVector(const std::string& prop_name, const Vector3f& vec, ShadingTreePropertyEmbedType embedType)
{
    if (checkIfEmbed(embedType)) {
        return "make_vec3(" + std::to_string(vec.x()) + ", " + std::to_string(vec.y()) + ", " + std::to_string(vec.z()) + ")";
    } else {
        const std::string id                        = currentClosureID() + "_" + LoaderUtils::escapeIdentifier(prop_name);
        const size_t reg_id                         = mContext.LocalRegistry.VectorParameters.size();
        mContext.LocalRegistry.VectorParameters[id] = vec;

        mHeaderLines.push_back("let var_vec_" + id + " = device.get_local_parameter_vec3(" + std::to_string(reg_id) + ");");
        return "var_vec_" + id;
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