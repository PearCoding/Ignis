#include "ShadingTree.h"
#include "Loader.h"
#include "LoaderTexture.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "StringUtils.h"
#include "device/Device.h"
#include "shader/BakeShader.h"
#include "shader/ScriptCompiler.h"

#include <algorithm>
#include <cctype>
#include <locale>

namespace IG {
ShadingTree::ShadingTree(LoaderContext& ctx)
    : mContext(ctx)
    , mTranspiler(*this)
    , mForceSpecialization(false)
{
    beginClosure("_root");
    setupGlobalParameters();
}

void ShadingTree::signalError()
{
    mContext.signalError();
}

void ShadingTree::setupGlobalParameters()
{
    auto& reg = mContext.GlobalRegistry;
    // Register all available user parameters
    for (const auto& pair : mContext.Options.Scene->parameters()) {
        const auto param       = pair.second;
        const std::string type = param->pluginType();

        if (type == "number") {
            const auto prop                 = param->property("value");
            reg.FloatParameters[pair.first] = handleGlobalParameterNumber(pair.first, prop);
            const std::string param_name    = "param_f32_" + whitespace_escaped(pair.first);
            mHeaderLines.push_back("  let " + param_name + " = registry::get_global_parameter_f32(\"" + pair.first + "\", 0); maybe_unused(" + param_name + ");\n");
            mTranspiler.registerCustomVariableNumber(pair.first, param_name);
        } else if (type == "vector") {
            const auto prop                  = param->property("value");
            reg.VectorParameters[pair.first] = handleGlobalParameterVector(pair.first, prop);
            const std::string param_name     = "param_vec3_" + whitespace_escaped(pair.first);
            mHeaderLines.push_back("  let " + param_name + " = registry::get_global_parameter_vec3(\"" + pair.first + "\", vec3_expand(0)); maybe_unused(" + param_name + ");\n");
            mTranspiler.registerCustomVariableVector(pair.first, param_name);
        } else if (type == "color") {
            const auto prop                 = param->property("value");
            reg.ColorParameters[pair.first] = handleGlobalParameterColor(pair.first, prop);
            const std::string param_name    = "param_color_" + whitespace_escaped(pair.first);
            mHeaderLines.push_back("  let " + param_name + " = registry::get_global_parameter_color(\"" + pair.first + "\", color_builtins::black); maybe_unused(" + param_name + ");\n");
            mTranspiler.registerCustomVariableColor(pair.first, param_name);
        }
    }
}

float ShadingTree::handleGlobalParameterNumber(const std::string& name, const SceneProperty& prop)
{
    float value = 0;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid value type" << std::endl;
        break;
    case SceneProperty::PT_NONE:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has no value!" << std::endl;
        break;
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
        value = prop.getNumber();
        break;
    case SceneProperty::PT_STRING:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' is using PExpr expressions, which is not supported!" << std::endl;
        break;
    }
    return value;
}

Vector3f ShadingTree::handleGlobalParameterVector(const std::string& name, const SceneProperty& prop)
{
    Vector3f value = Vector3f::Zero();
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid value type" << std::endl;
        break;
    case SceneProperty::PT_NONE:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has no value!" << std::endl;
        break;
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
        value = Vector3f::Constant(prop.getNumber());
        break;
    case SceneProperty::PT_VECTOR3:
        value = prop.getVector3();
        break;
    case SceneProperty::PT_STRING:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' is using PExpr expressions, which is not supported!" << std::endl;
        break;
    }
    return value;
}

Vector4f ShadingTree::handleGlobalParameterColor(const std::string& name, const SceneProperty& prop)
{
    const Vector3f a = handleGlobalParameterVector(name, prop);
    return Vector4f(a.x(), a.y(), a.z(), 1); // TODO: Support alpha?
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

std::string ShadingTree::handlePropertyNumber(const std::string& name, const SceneProperty& prop, const NumberOptions& options)
{
    switch (prop.type()) {
    default:
    case SceneProperty::PT_NONE:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        return {};
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
        return acquireNumber(name, prop.getNumber(), options);
    case SceneProperty::PT_VECTOR3:
        IG_LOG(L_WARNING) << "Parameter '" << name << "' expects a number but a color was given. Using average instead" << std::endl;
        return "color_average(" + acquireColor(name, prop.getVector3(), mapToColorOptions(options)) + ")";
    case SceneProperty::PT_STRING:
        return handleTexture(name, prop.getString(), false); // TODO: Map options
    }
}

void ShadingTree::addNumber(const std::string& name, const SceneObject& obj, const std::optional<float>& def, const NumberOptions& options)
{
    if (hasParameter(name)) {
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << name << "'" << std::endl;
        signalError();
    }

    const auto prop = obj.property(name);

    std::string inline_str;
    if (prop.type() == SceneProperty::PT_NONE) {
        if (!def.has_value())
            return;
        inline_str = acquireNumber(name, def.value(), options);
    } else {
        inline_str = handlePropertyNumber(name, prop, options);
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addColor(const std::string& name, const SceneObject& obj, const std::optional<Vector3f>& def, const ColorOptions& options)
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
    case SceneProperty::PT_NONE:
        if (!def.has_value())
            return;
        inline_str = acquireColor(name, def.value(), options);
        break;
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
        inline_str = "make_gray_color(" + acquireNumber(name, prop.getNumber(), mapToNumberOptions(options)) + ")";
        break;
    case SceneProperty::PT_VECTOR3:
        inline_str = acquireColor(name, prop.getVector3(), options);
        break;
    case SceneProperty::PT_STRING:
        inline_str = handleTexture(name, prop.getString(), true); // TODO: Map options
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addVector(const std::string& name, const SceneObject& obj, const std::optional<Vector3f>& def, const VectorOptions& options)
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
    case SceneProperty::PT_NONE:
        if (!def.has_value())
            return;
        inline_str = acquireVector(name, def.value(), options);
        break;
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
        inline_str = "vec3_expand(" + acquireNumber(name, prop.getNumber(), mapToNumberOptions(options)) + ")";
        break;
    case SceneProperty::PT_VECTOR3:
        inline_str = acquireVector(name, prop.getVector3(), options);
        break;
    case SceneProperty::PT_STRING:
        inline_str = "color_to_vec3(" + handleTexture(name, prop.getString(), true) + ")"; // TODO: Map options
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

// Only use this if no basic color information suffices
void ShadingTree::addTexture(const std::string& name, const SceneObject& obj, const std::optional<Vector3f>& def, const TextureOptions& options)
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
    case SceneProperty::PT_NONE:
        if (!def.has_value())
            return;
        inline_str = "make_constant_texture(" + acquireColor(name, def.value(), mapToColorOptions(options)) + ")";
        break;
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
        inline_str = "make_constant_texture(make_gray_color(" + acquireNumber(name, prop.getNumber(), mapToNumberOptions(options)) + "))";
        break;
    case SceneProperty::PT_VECTOR3:
        inline_str = "make_constant_texture(" + acquireColor(name, prop.getVector3(), mapToColorOptions(options)) + ")";
        break;
    case SceneProperty::PT_STRING: {
        std::string tex_func = handleTexture(name, prop.getString(), true);
        inline_str           = "@|ctx:ShadingContext|->Color{maybe_unused(ctx); " + tex_func + "}";
    } break;
    }

    currentClosure().Parameters[name] = inline_str;
}

float ShadingTree::computeNumber(const std::string& name, const SceneObject& obj, float def)
{
    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case SceneProperty::PT_NONE:
        return def;
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
        return prop.getNumber();
    case SceneProperty::PT_VECTOR3:
        return prop.getVector3().mean();
    case SceneProperty::PT_STRING: {
        if (const auto it = mContext.Cache->ExprComputation.find(prop.getString()); it != mContext.Cache->ExprComputation.end())
            return it->second.mean();

        IG_LOG(L_DEBUG) << "Computing number for expression '" << prop.getString() << "'" << std::endl;
        LoaderContext ctx_copy = mContext.copyForBake();
        return ShadingTree(ctx_copy).bakeTextureExpressionAverage(name, prop.getString(), Vector3f::Constant(def)).mean();
    }
    }
}

Vector3f ShadingTree::computeColor(const std::string& name, const SceneObject& obj, const Vector3f& def)
{
    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case SceneProperty::PT_NONE:
        return def;
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
        return Vector3f::Constant(prop.getNumber());
    case SceneProperty::PT_VECTOR3:
        return prop.getVector3();
    case SceneProperty::PT_STRING: {
        if (const auto it = mContext.Cache->ExprComputation.find(prop.getString()); it != mContext.Cache->ExprComputation.end())
            return it->second;

        IG_LOG(L_DEBUG) << "Computing color for expression '" << prop.getString() << "'" << std::endl;
        LoaderContext ctx_copy = mContext.copyForBake();
        return ShadingTree(ctx_copy).bakeTextureExpressionAverage(name, prop.getString(), def);
    }
    }
}

ShadingTree::BakeOutputTexture ShadingTree::bakeTexture(const std::string& name, const SceneObject& obj, const std::optional<Vector3f>& def, const TextureBakeOptions& options)
{
    // options only affect bake process with PExpr expressions

    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case SceneProperty::PT_NONE:
        if (!def.has_value())
            return {};
        return std::make_shared<Image>(Image::createSolidImage(Vector4f(def->x(), def->y(), def->z(), 1)));
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
        return std::make_shared<Image>(Image::createSolidImage(Vector4f(prop.getNumber(), prop.getNumber(), prop.getNumber(), 1)));
    case SceneProperty::PT_VECTOR3: {
        const Vector3f c = prop.getVector3();
        return std::make_shared<Image>(Image::createSolidImage(Vector4f(c.x(), c.y(), c.z(), 1)));
    }
    case SceneProperty::PT_STRING: {
        IG_LOG(L_DEBUG) << "Baking property '" << name << "'" << std::endl;
        LoaderContext ctx_copy = mContext.copyForBake();
        return ShadingTree(ctx_copy).bakeTextureExpression(name, prop.getString(), options);
    }
    }
}

static inline std::optional<float> tryExtractFloat(const std::string& str)
{
    std::string c_str = str;
    string_trim(c_str);

    try {
        size_t idx = 0;
        float f    = std::stof(c_str, &idx);
        if (idx != c_str.size())
            return std::nullopt;
        else
            return std::make_optional(f);
    } catch (...) {
        return std::nullopt;
    }
}

Vector3f ShadingTree::computeConstantColor(const std::string& name, const Transpiler::Result& result)
{
    IG_UNUSED(name);

    // Handle simple case where it is just a number
    const auto potential_number = tryExtractFloat(result.Expr);
    if (potential_number)
        return Vector3f::Constant(*potential_number);

    std::string expr_art = result.Expr;
    if (result.ScalarOutput)
        expr_art = "make_gray_color(" + expr_art + ")";

    // Constant expression with no ctx and textures
    const std::string script = BakeShader::setupConstantColor("  let main_func = @|| " + expr_art + ";");

    void* shader  = mContext.Options.Compiler->compile(mContext.Options.Compiler->prepare(script), "ig_constant_color");
    auto callback = reinterpret_cast<BakeShader::ConstantColorFunc>(shader);
    Vector4f color;
    callback(&color.x(), &color.y(), &color.z(), &color.w());
    return color.block<3, 1>(0, 0);
}

Image ShadingTree::computeImage(const std::string& name, const Transpiler::Result& result, const TextureBakeOptions& options)
{
    bool warn = false;
    for (const std::string& var : result.Variables) {
        if (var != "uv")
            warn = true;
    }

    if (warn)
        IG_LOG(L_WARNING) << "Given expression for '" << name << "' contains variables outside `uv`. Baking process might be incomplete" << std::endl;

    std::stringstream inner_script;
    for (const auto& tex : result.Textures) {
        inner_script << loadTexture(tex);
    }

    std::string expr_art = result.Expr;
    if (result.ScalarOutput)
        expr_art = "make_gray_color(" + expr_art + ")";

    inner_script << "  let main_func = @|ctx:ShadingContext|->Color{maybe_unused(ctx); " + expr_art + "};" << std::endl;

    const std::string script = BakeShader::setupTexture2d(mContext, inner_script.str(), options.Width, options.Height);

    void* shader = mContext.Options.Compiler->compile(mContext.Options.Compiler->prepare(script), "ig_bake_shader");

    Image image          = Image::createSolidImage(Vector4f::Zero(), options.Width, options.Height);
    const auto resources = mContext.generateResourceMap();
    mContext.Options.Device->bake(ShaderOutput<void*>{ shader, mContext.LocalRegistry }, &resources, image.pixels.get());
    return image;
}

ShadingTree::BakeOutputTexture ShadingTree::bakeTextureExpression(const std::string& name, const std::string& expr, const TextureBakeOptions& options)
{
    auto res = mTranspiler.transpile(expr);

    if (!res.has_value()) {
        return {};
    } else {
        const auto& result = res.value();

        if (result.Textures.empty() && result.Variables.empty()) {
            if (options.SkipConstant)
                return {};

            const Vector3f color = computeConstantColor(name, result);
            return std::make_shared<Image>(Image::createSolidImage(Vector4f(color.x(), color.y(), color.z(), 1)));
        }

        Image image = computeImage(name, result, options);
        return std::make_shared<Image>(std::move(image));
    }
}

Vector3f ShadingTree::bakeTextureExpressionAverage(const std::string& name, const std::string& expr, const Vector3f& def)
{
    auto res = mTranspiler.transpile(expr);

    if (!res.has_value()) {
        return def;
    } else {
        const auto& result = res.value();

        if (result.Textures.empty() && result.Variables.empty()) {
            const Vector3f color = computeConstantColor(name, result);

            mContext.Cache->ExprComputation[expr] = color;
            return color;
        }

        const Image image                     = computeImage(name, result, TextureBakeOptions{ 16, 16, true });
        const Vector4f average                = image.computeAverage();
        mContext.Cache->ExprComputation[expr] = average.block<3, 1>(0, 0);
        return average.block<3, 1>(0, 0);
    }
}

void ShadingTree::beginClosure(const std::string& name)
{
    mClosures.emplace_back(Closure{ name, getClosureID(name), {} });
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

void ShadingTree::registerTextureUsage(const std::string& name)
{
    if (mLoadedTextures.count(name) == 0) {
        const std::string res = loadTexture(name);
        if (res.empty()) // Due to some error this might happen
            return;
        mHeaderLines.push_back(res);
        mLoadedTextures.insert(name);
    }
}

std::string ShadingTree::loadTexture(const std::string& tex_name)
{
    return mContext.Textures->generate(tex_name, *this);
}

std::string ShadingTree::handleTexture(const std::string& prop_name, const std::string& expr, bool needColor)
{
    IG_UNUSED(prop_name);

    auto res = mTranspiler.transpile(expr);

    if (!res.has_value()) {
        if (needColor)
            return "color_builtins::pink/*Error*/";
        else
            return "0:f32/*Error*/";
    } else {
        for (const auto& tex : res.value().Textures)
            registerTextureUsage(tex);

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

bool ShadingTree::checkIfEmbed(float val, const NumberOptions& options) const
{
    switch (options.EmbedType) {
    case EmbedType::Structural:
        return true;
    case EmbedType::Dynamic:
        return false;
    default:
    case EmbedType::Default:
        if (mForceSpecialization || mContext.Options.ForceSpecialization)
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
        if (mForceSpecialization || mContext.Options.ForceSpecialization)
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
        if (mForceSpecialization || mContext.Options.ForceSpecialization)
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