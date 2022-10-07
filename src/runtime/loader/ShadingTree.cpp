#include "ShadingTree.h"
#include "Loader.h"
#include "LoaderTexture.h"
#include "LoaderUtils.h"
#include "Logger.h"

namespace IG {
ShadingTree::ShadingTree(LoaderContext& ctx)
    : mContext(ctx)
    , mTranspiler(*this)
    , mForceSpecialization(false)
{
    beginClosure("_root");
}

void ShadingTree::signalError()
{
    mContext.signalError();
}

// ------------------ Number
static inline ShadingTree::NumberOptions mapToNumberOptions(const ShadingTree::VectorOptions& options)
{
    return ShadingTree::NumberOptions{
        options.EmbedType,
        options.SpecializeZero,
        options.SpecializeOne
    };
}

static inline ShadingTree::NumberOptions mapToNumberOptions(const ShadingTree::ColorOptions& options)
{
    return ShadingTree::NumberOptions{
        options.EmbedType,
        options.SpecializeBlack,
        options.SpecializeWhite
    };
}

static inline ShadingTree::NumberOptions mapToNumberOptions(const ShadingTree::TextureOptions& options)
{
    return ShadingTree::NumberOptions{
        options.EmbedType,
        true, // Really?
        true
    };
}

// ------------------ Color
static inline ShadingTree::ColorOptions mapToColorOptions(const ShadingTree::NumberOptions& options)
{
    return ShadingTree::ColorOptions{
        options.EmbedType,
        options.SpecializeZero,
        options.SpecializeOne
    };
}

static inline ShadingTree::ColorOptions mapToColorOptions(const ShadingTree::TextureOptions& options)
{
    return ShadingTree::ColorOptions{
        options.EmbedType,
        true, // Really?
        true
    };
}

float ShadingTree::computeNumber(const std::string& name, const Parser::Object& obj, float def) const
{
    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case Parser::PT_NONE:
        return def;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        return prop.getNumber();
    case Parser::PT_VECTOR3:
        return prop.getVector3().mean();
    case Parser::PT_STRING:
        return approxTexture(name, prop.getString(), Vector3f::Constant(def)).mean();
    }
}

Vector3f ShadingTree::computeColor(const std::string& name, const Parser::Object& obj, const Vector3f& def) const
{
    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case Parser::PT_NONE:
        return def;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        return Vector3f::Constant(prop.getNumber());
    case Parser::PT_VECTOR3:
        return prop.getVector3();
    case Parser::PT_STRING:
        return approxTexture(name, prop.getString(), def);
    }
}

void ShadingTree::addNumber(const std::string& name, const Parser::Object& obj, float def, bool hasDef, const NumberOptions& options)
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
        inline_str = acquireNumber(name, def, options);
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        inline_str = acquireNumber(name, prop.getNumber(), options);
        break;
    case Parser::PT_VECTOR3:
        IG_LOG(L_WARNING) << "Parameter '" << name << "' expects a number but a color was given. Using average instead" << std::endl;
        inline_str = "color_average(" + acquireColor(name, prop.getVector3(), mapToColorOptions(options)) + ")";
        break;
    case Parser::PT_STRING:
        inline_str = handleTexture(name, prop.getString(), false); // TODO: Map options
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addColor(const std::string& name, const Parser::Object& obj, const Vector3f& def, bool hasDef, const ColorOptions& options)
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
        inline_str = acquireColor(name, def, options);
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        inline_str = "make_gray_color(" + acquireNumber(name, prop.getNumber(), mapToNumberOptions(options)) + ")";
        break;
    case Parser::PT_VECTOR3:
        inline_str = acquireColor(name, prop.getVector3(), options);
        break;
    case Parser::PT_STRING:
        inline_str = handleTexture(name, prop.getString(), true); // TODO: Map options
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addVector(const std::string& name, const Parser::Object& obj, const Vector3f& def, bool hasDef, const VectorOptions& options)
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
        inline_str = acquireVector(name, def, options);
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        inline_str = "vec3_expand(" + acquireNumber(name, prop.getNumber(), mapToNumberOptions(options)) + ")";
        break;
    case Parser::PT_VECTOR3:
        inline_str = acquireVector(name, prop.getVector3(), options);
        break;
    case Parser::PT_STRING:
        inline_str = "color_to_vec3(" + handleTexture(name, prop.getString(), true) + ")"; // TODO: Map options
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

// Only use this if no basic color information suffices
void ShadingTree::addTexture(const std::string& name, const Parser::Object& obj, bool hasDef, const TextureOptions& options)
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
        inline_str = "make_constant_texture(make_gray_color(" + acquireNumber(name, prop.getNumber(), mapToNumberOptions(options)) + "))";
        break;
    case Parser::PT_VECTOR3:;
        inline_str = "make_constant_texture(" + acquireColor(name, prop.getVector3(), mapToColorOptions(options)) + ")";
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
    mClosures.emplace_back(Closure{ name, getClosureID(name), {} });
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
            mHeaderLines.push_back("  let tex_" + getClosureID(name) + " = make_invalid_texture();\n");
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

Vector3f ShadingTree::approxTexture(const std::string& prop_name, const std::string& expr, const Vector3f& def) const
{
    // TODO
    IG_UNUSED(prop_name);
    IG_UNUSED(expr);
    return def;
}

bool ShadingTree::checkIfEmbed(float val, const NumberOptions& options) const
{
    switch (options.EmbedType) {
    case EmbedType::Structural:
        return true;
    case EmbedType::Dynamic:
        return false;
    default:
    case EmbedType::Default:
        if (mForceSpecialization || context().ForceShadingTreeSpecialization)
            return true;
        else if (options.SpecializeZero && std::abs(val) <= FltEps)
            return true;
        else if (options.SpecializeOne && std::abs(val - 1) <= FltEps)
            return true;
        else
            return false;
    }
}

bool ShadingTree::checkIfEmbed(const Vector3f& color, const ColorOptions& options) const
{
    switch (options.EmbedType) {
    case EmbedType::Structural:
        return true;
    case EmbedType::Dynamic:
        return false;
    default:
    case EmbedType::Default:
        if (mForceSpecialization || context().ForceShadingTreeSpecialization)
            return true;
        else if (options.SpecializeBlack && color.isZero(FltEps))
            return true;
        else if (options.SpecializeWhite && color.isOnes(FltEps))
            return true;
        else
            return false;
    }
}

bool ShadingTree::checkIfEmbed(const Vector3f& vec, const VectorOptions& options) const
{
    switch (options.EmbedType) {
    case EmbedType::Structural:
        return true;
    case EmbedType::Dynamic:
        return false;
    default:
    case EmbedType::Default:
        if (mForceSpecialization || context().ForceShadingTreeSpecialization)
            return true;
        else if (options.SpecializeZero && vec.isZero(FltEps))
            return true;
        else if (options.SpecializeOne && vec.isOnes(FltEps))
            return true;
        else if (options.SpecializeUnit
                 && (vec.cwiseAbs().isApprox(Vector3f::UnitX(), FltEps) || vec.cwiseAbs().isApprox(Vector3f::UnitY(), FltEps) || vec.cwiseAbs().isApprox(Vector3f::UnitZ(), FltEps)))
            return true;
        else
            return false;
    }
}

std::string ShadingTree::acquireNumber(const std::string& prop_name, float number, const NumberOptions& options)
{
    if (checkIfEmbed(number, options)) {
        return std::to_string(number);
    } else {
        const std::string id                       = currentClosureID() + "_" + LoaderUtils::escapeIdentifier(prop_name);
        mContext.LocalRegistry.FloatParameters[id] = number;

        mHeaderLines.push_back("  let var_num_" + id + " = device.get_local_parameter_f32(\"" + id + "\", 0);\n");
        return "var_num_" + id;
    }
}

std::string ShadingTree::acquireColor(const std::string& prop_name, const Vector3f& color, const ColorOptions& options)
{
    if (checkIfEmbed(color, options)) {
        return "make_color(" + std::to_string(color.x()) + ", " + std::to_string(color.y()) + ", " + std::to_string(color.z()) + ", 1)";
    } else {
        const std::string id                       = currentClosureID() + "_" + LoaderUtils::escapeIdentifier(prop_name);
        mContext.LocalRegistry.ColorParameters[id] = Vector4f(color.x(), color.y(), color.z(), 1);

        mHeaderLines.push_back("  let var_color_" + id + " = device.get_local_parameter_color(\"" + id + "\", color_builtins::black);\n");
        return "var_color_" + id;
    }
}

std::string ShadingTree::acquireVector(const std::string& prop_name, const Vector3f& vec, const VectorOptions& options)
{
    if (checkIfEmbed(vec, options)) {
        return "make_vec3(" + std::to_string(vec.x()) + ", " + std::to_string(vec.y()) + ", " + std::to_string(vec.z()) + ")";
    } else {
        const std::string id                        = currentClosureID() + "_" + LoaderUtils::escapeIdentifier(prop_name);
        mContext.LocalRegistry.VectorParameters[id] = vec;

        mHeaderLines.push_back("  let var_vec_" + id + " = device.get_local_parameter_vec3(\"" + id + "\", vec3_expand(0));\n");
        return "var_vec_" + id;
    }
}

std::string ShadingTree::getClosureID(const std::string& name)
{
    auto it = mIDMap.find(name);
    if (it != mIDMap.end())
        return std::to_string(it->second);

    size_t id    = mIDMap.size();
    mIDMap[name] = id;
    return std::to_string(id);
}
} // namespace IG