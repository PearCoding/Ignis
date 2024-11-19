#include "ShadingTree.h"
#include "Loader.h"
#include "LoaderTexture.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "StringUtils.h"
#include "device/IRenderDevice.h"
#include "shader/BakeShader.h"
#include "shader/ScriptCompiler.h"

#include <algorithm>
#include <cctype>
#include <locale>

namespace IG {
ShadingTree::ShadingTree(LoaderContext& ctx)
    : mContext(ctx)
    , mTranspiler(*this)
    , mSpecialization(RuntimeOptions::SpecializationMode::Default)
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

        if (type == "number" || type == "int" || type == "integer") {
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

std::string ShadingTree::handlePropertyInteger(const std::string& name, const SceneProperty& prop, const IntegerOptions& options)
{
    switch (prop.type()) {
    default:
    case SceneProperty::PT_NONE:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        return {};
    case SceneProperty::PT_INTEGER:
        return acquireInteger(name, prop.getInteger(), options);
    case SceneProperty::PT_NUMBER:
        IG_LOG(L_WARNING) << "Parameter '" << name << "' expects an integer but a number was given. Casting to `int` instead" << std::endl;
        return acquireInteger(name, (int)prop.getNumber(), options);
    case SceneProperty::PT_VECTOR3:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' expects an integer but a color was given. Can not cast this" << std::endl;
        signalError();
        return {};
    case SceneProperty::PT_STRING:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' expects an integer but a texture was given. Can not cast this" << std::endl;
        signalError();
        return {};
    }
}

void ShadingTree::addInteger(const std::string& name, SceneObject& obj, const std::optional<int>& def, const IntegerOptions& options)
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
        inline_str = acquireInteger(name, def.value(), options);
    } else {
        inline_str = handlePropertyInteger(name, prop, options);
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addComputedInteger(const std::string& prop_name, int number, const IntegerOptions& options)
{
    if (hasParameter(prop_name)) {
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << prop_name << "'" << std::endl;
        signalError();
    }
    currentClosure().Parameters[prop_name] = acquireInteger(prop_name, number, options);
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
        if (const auto number = bakeSimpleNumber(name, prop.getString()); number.has_value())
            return acquireNumber(name, *number, options);
        else
            return handleTexture(name, prop.getString(), false); // TODO: Map options
    }
}

void ShadingTree::addNumber(const std::string& name, SceneObject& obj, const std::optional<float>& def, const NumberOptions& options)
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

void ShadingTree::addComputedNumber(const std::string& prop_name, float number, const NumberOptions& options)
{
    if (hasParameter(prop_name)) {
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << prop_name << "'" << std::endl;
        signalError();
    }
    currentClosure().Parameters[prop_name] = acquireNumber(prop_name, number, options);
}

void ShadingTree::addColor(const std::string& name, SceneObject& obj, const std::optional<Vector3f>& def, const ColorOptions& options)
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
        if (const auto color = bakeSimpleColor(name, prop.getString()); color.has_value())
            inline_str = acquireColor(name, *color, options);
        else
            inline_str = handleTexture(name, prop.getString(), true); // TODO: Map options
        break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addComputedColor(const std::string& prop_name, const Vector3f& color, const ColorOptions& options)
{
    if (hasParameter(prop_name)) {
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << prop_name << "'" << std::endl;
        signalError();
    }
    currentClosure().Parameters[prop_name] = acquireColor(prop_name, color, options);
}

void ShadingTree::addVector(const std::string& name, SceneObject& obj, const std::optional<Vector3f>& def, const VectorOptions& options)
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

void ShadingTree::addComputedVector(const std::string& prop_name, const Vector3f& vec, const VectorOptions& options)
{
    if (hasParameter(prop_name)) {
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << prop_name << "'" << std::endl;
        signalError();
    }
    currentClosure().Parameters[prop_name] = acquireVector(prop_name, vec, options);
}

void ShadingTree::addComputedMatrix3(const std::string& prop_name, const Matrix3f& mat, const VectorOptions& options)
{
    addComputedVector(prop_name + "_c0", mat.col(0), options);
    addComputedVector(prop_name + "_c1", mat.col(1), options);
    addComputedVector(prop_name + "_c2", mat.col(2), options);
}

void ShadingTree::addComputedMatrix34(const std::string& prop_name, const Matrix34f& mat, const VectorOptions& options)
{
    addComputedVector(prop_name + "_c0", mat.col(0), options);
    addComputedVector(prop_name + "_c1", mat.col(1), options);
    addComputedVector(prop_name + "_c2", mat.col(2), options);
    addComputedVector(prop_name + "_c3", mat.col(3), options);
}

// void ShadingTree::addComputedMatrix44(const std::string& prop_name, const Matrix4f& mat, const VectorOptions& options)
// {
//     addComputedVector(prop_name + "_c0", mat.col(0), options);
//     addComputedVector(prop_name + "_c1", mat.col(1), options);
//     addComputedVector(prop_name + "_c2", mat.col(2), options);
//     addComputedVector(prop_name + "_c3", mat.col(3), options);
// }

// Only use this if no basic color information suffices
void ShadingTree::addTexture(const std::string& name, SceneObject& obj, const std::optional<Vector3f>& def, const TextureOptions& options)
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

ShadingTree::BakeOutputNumber ShadingTree::computeNumber(const std::string& name, SceneObject& obj, float def, const GenericBakeOptions& options)
{
    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case SceneProperty::PT_NONE:
        return BakeOutputNumber::AsConstant(def);
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
        return BakeOutputNumber::AsConstant(prop.getNumber());
    case SceneProperty::PT_VECTOR3:
        return BakeOutputNumber::AsConstant(prop.getVector3().mean());
    case SceneProperty::PT_STRING: {
        IG_LOG(L_DEBUG) << "Computing number for expression '" << prop.getString() << "'" << std::endl;
        const auto color_output = bakeTextureExpressionAverage(name, prop.getString(), Vector3f::Constant(def), options);
        return BakeOutputNumber{ color_output.Value.mean(), color_output.WasConstant };
    }
    }
}

ShadingTree::BakeOutputColor ShadingTree::computeColor(const std::string& name, SceneObject& obj, const Vector3f& def, const GenericBakeOptions& options)
{
    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case SceneProperty::PT_NONE:
        return BakeOutputColor::AsConstant(def);
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
        return BakeOutputColor::AsConstant(Vector3f::Constant(prop.getNumber()));
    case SceneProperty::PT_VECTOR3:
        return BakeOutputColor::AsConstant(prop.getVector3());
    case SceneProperty::PT_STRING: {
        IG_LOG(L_DEBUG) << "Computing color for expression '" << prop.getString() << "'" << std::endl;
        return bakeTextureExpressionAverage(name, prop.getString(), def, options);
    }
    }
}

ShadingTree::BakeOutputTexture ShadingTree::bakeTexture(const std::string& name, SceneObject& obj, const std::optional<Vector3f>& def, const TextureBakeOptions& options)
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
        return bakeTextureExpression(name, prop.getString(), options);
    }
    }
}

std::pair<size_t, size_t> ShadingTree::computeTextureResolution(const std::string& name, SceneObject& obj)
{
    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case SceneProperty::PT_NONE:
    case SceneProperty::PT_INTEGER:
    case SceneProperty::PT_NUMBER:
    case SceneProperty::PT_VECTOR3:
        return { 1, 1 };
    case SceneProperty::PT_STRING: {
        const std::string expr = prop.getString();
        if (const auto it = mContext.Cache->ExprResolution.find(expr); it != mContext.Cache->ExprResolution.end())
            return it->second;

        IG_LOG(L_DEBUG) << "Computing resolution of property '" << name << "'" << std::endl;
        const auto res                       = computeTextureResolution(name, expr);
        mContext.Cache->ExprResolution[expr] = res;
        return res;
    }
    }
}

static inline bool isTexVariable(const std::string& var)
{
    return var == "uv" || var == "uvw";
}

std::pair<size_t, size_t> ShadingTree::computeTextureResolution(const std::string& name, const std::string& expr)
{
    const auto res = mTranspiler.transpile(expr);
    if (!res.has_value())
        return { 1, 1 };
    else
        return computeTextureResolution(name, res.value());
}

std::pair<size_t, size_t> ShadingTree::computeTextureResolution(const std::string&, const Transpiler::Result& result)
{
    std::pair<size_t, size_t> res = { 1, 1 };
    for (const auto& tex : result.Textures) {
        const auto tex_res = mContext.Textures->computeResolution(tex, *this);

        res.first  = std::max(res.first, tex_res.first);
        res.second = std::max(res.second, tex_res.second);
    }

    return res;
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
    const std::string script = BakeShader::setupConstantColor("  let main_func = @|| {" + pullHeader() + expr_art + "};");

    void* shader  = mContext.Options.Compiler->compile(mContext.Options.Compiler->prepare(script), "ig_constant_color");
    auto callback = reinterpret_cast<BakeShader::ConstantColorFunc>(shader);
    Vector4f color;
    callback(&color.x(), &color.y(), &color.z(), &color.w());
    return color.block<3, 1>(0, 0);
}

Image ShadingTree::computeImage(const std::string& name, const Transpiler::Result& result, const TextureBakeOptions& options)
{
    // Note: Already inside a copied tree for baking purposes!

    bool warn = false;
    for (const std::string& var : result.Variables) {
        if (!isTexVariable(var))
            warn = true;
    }

    if (warn)
        IG_LOG(L_WARNING) << "Given expression for '" << name << "' contains variables outside `uv`. Baking process might be incomplete" << std::endl;

    std::stringstream inner_script;
    for (const auto& tex : result.Textures)
        inner_script << loadTexture(tex);
    inner_script << pullHeader() << std::endl;

    std::string expr_art = result.Expr;
    if (result.ScalarOutput)
        expr_art = "make_gray_color(" + expr_art + ")";

    inner_script << "  let main_func = @|ctx:ShadingContext|->Color{maybe_unused(ctx); " + expr_art + "};" << std::endl;

    // Compute fitting resolution if needed
    size_t width  = std::max(options.MaxWidth, options.MinWidth);
    size_t height = std::max(options.MaxHeight, options.MinHeight);
    if (options.MaxWidth != options.MinWidth || options.MaxHeight != options.MinHeight) {
        const auto res = computeTextureResolution(name, result);
        width          = std::max(options.MinWidth, res.first);
        height         = std::max(options.MinHeight, res.second);
        if (options.MaxWidth > 0)
            width = std::min(options.MaxWidth, width);
        if (options.MaxHeight > 0)
            height = std::min(options.MaxHeight, height);
    }
    // Ensure width is at minimum 1
    width  = std::max<size_t>(1, width);
    height = std::max<size_t>(1, height);

    const std::string script = BakeShader::setupTexture2d(mContext, inner_script.str(), width, height);

    void* shader = mContext.Options.Compiler->compile(mContext.Options.Compiler->prepare(script), "ig_bake_shader");
    if (shader == nullptr)
        return Image::createSolidImage(Vector4f::Zero(), width, height);

    Image image          = Image::createSolidImage(Vector4f::Zero(), width, height);
    const auto resources = mContext.generateResourceMap();
    mContext.Options.Device->bake(ShaderOutput<void*>{ shader, std::make_shared<ParameterSet>(mContext.LocalRegistry) }, &resources, image.pixels.get());
    return image;
}

ShadingTree::BakeOutputTexture ShadingTree::bakeTextureExpression(const std::string& name, const std::string& expr, const TextureBakeOptions& options)
{
    auto copyCtx  = mContext.copyForBake();
    auto copyTree = ShadingTree(copyCtx);

    auto res = copyTree.mTranspiler.transpile(expr);

    if (!res.has_value()) {
        return {};
    } else {
        const auto& result = res.value();

        if (result.isSimple()) {
            if (options.SkipConstant)
                return {};

            const Vector3f color = copyTree.computeConstantColor(name, result);
            return std::make_shared<Image>(Image::createSolidImage(Vector4f(color.x(), color.y(), color.z(), 1)));
        }

        Image image = copyTree.computeImage(name, result, options);
        return std::make_shared<Image>(std::move(image));
    }
}

ShadingTree::BakeOutputColor ShadingTree::bakeTextureExpressionAverage(const std::string& name, const std::string& expr, const Vector3f& def, const GenericBakeOptions& options)
{
    const std::string expr_key = expr + "_avg";

    if (const auto it = mContext.Cache->ExprComputation.find(expr_key); it != mContext.Cache->ExprComputation.end()) {
        const auto output = std::any_cast<BakeOutputColor>(it->second);
        return BakeOutputColor{ output.Value, output.WasConstant };
    }

    auto copyCtx  = mContext.copyForBake();
    auto copyTree = ShadingTree(copyCtx);

    auto res = copyTree.mTranspiler.transpile(expr);

    if (!res.has_value()) {
        return BakeOutputColor::AsConstant(def);
    } else {
        const auto& result = res.value();

        if (result.isSimple()) {
            const auto color = BakeOutputColor::AsConstant(copyTree.computeConstantColor(name, result));

            mContext.Cache->ExprComputation[expr_key] = color;
            return color;
        } else if (options.SkipTextures) {
            return BakeOutputColor::AsConstant(def);
        } else {
            const Image image                         = copyTree.computeImage(name, result, TextureBakeOptions{ 0, 0, 1, 1, true });
            const Vector4f average                    = image.computeAverage();
            const auto color                          = BakeOutputColor::AsConstant(average.block<3, 1>(0, 0));
            mContext.Cache->ExprComputation[expr_key] = color;
            return color;
        }
    }
}

std::optional<float> ShadingTree::bakeSimpleNumber(const std::string& name, const std::string& expr)
{
    const std::string expr_key = expr + "_num";

    if (const auto it = mContext.Cache->ExprComputation.find(expr_key); it != mContext.Cache->ExprComputation.end())
        return std::any_cast<BakeOutputColor>(it->second).Value.mean();

    auto copyCtx  = mContext.copyForBake();
    auto copyTree = ShadingTree(copyCtx);

    const auto res = copyTree.mTranspiler.transpile(expr);

    if (!res.has_value()) {
        return std::nullopt;
    } else {
        const auto& result = res.value();

        if (result.isSimple()) {
            const auto color = BakeOutputColor::AsConstant(copyTree.computeConstantColor(name, result));

            mContext.Cache->ExprComputation[expr_key] = color;
            return color.Value.mean();
        } else {
            return std::nullopt;
        }
    }
}

std::optional<Vector3f> ShadingTree::bakeSimpleColor(const std::string& name, const std::string& expr)
{
    const std::string expr_key = expr + "_color";

    if (const auto it = mContext.Cache->ExprComputation.find(expr_key); it != mContext.Cache->ExprComputation.end())
        return std::any_cast<BakeOutputColor>(it->second).Value;

    auto copyCtx  = mContext.copyForBake();
    auto copyTree = ShadingTree(copyCtx);

    const auto res = copyTree.mTranspiler.transpile(expr);

    if (!res.has_value()) {
        return std::nullopt;
    } else {
        const auto& result = res.value();

        if (result.isSimple()) {
            const auto color = BakeOutputColor::AsConstant(copyTree.computeConstantColor(name, result));

            mContext.Cache->ExprComputation[expr_key] = color;
            return color.Value;
        } else {
            return std::nullopt;
        }
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

std::string ShadingTree::getInlineMatrix3(const std::string& name)
{
    const std::string c0 = getInline(name + "_c0");
    const std::string c1 = getInline(name + "_c1");
    const std::string c2 = getInline(name + "_c2");
    return "make_mat3x3(" + c0 + "," + c1 + "," + c2 + ")";
}

std::string ShadingTree::getInlineMatrix34(const std::string& name)
{
    const std::string c0 = getInline(name + "_c0");
    const std::string c1 = getInline(name + "_c1");
    const std::string c2 = getInline(name + "_c2");
    const std::string c3 = getInline(name + "_c3");
    return "make_mat3x4(" + c0 + "," + c1 + "," + c2 + "," + c3 + ")";
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

bool ShadingTree::checkIfEmbed(int val, const IntegerOptions& options) const
{
    switch (options.EmbedType) {
    case EmbedType::Structural:
        return true;
    case EmbedType::Dynamic:
        return false;
    default:
    case EmbedType::Default:
        if (mSpecialization == RuntimeOptions::SpecializationMode::Force || mContext.Options.Specialization == RuntimeOptions::SpecializationMode::Force)
            return true;
        else if (mSpecialization == RuntimeOptions::SpecializationMode::Disable || mContext.Options.Specialization == RuntimeOptions::SpecializationMode::Disable)
            return false;
        else if (options.SpecializeZero && val == 0)
            return true;
        else if (options.SpecializeOne && val == 1)
            return true;
        else
            return false;
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
        if (mSpecialization == RuntimeOptions::SpecializationMode::Force || mContext.Options.Specialization == RuntimeOptions::SpecializationMode::Force)
            return true;
        else if (mSpecialization == RuntimeOptions::SpecializationMode::Disable || mContext.Options.Specialization == RuntimeOptions::SpecializationMode::Disable)
            return false;
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
        if (mSpecialization == RuntimeOptions::SpecializationMode::Force || mContext.Options.Specialization == RuntimeOptions::SpecializationMode::Force)
            return true;
        else if (mSpecialization == RuntimeOptions::SpecializationMode::Disable || mContext.Options.Specialization == RuntimeOptions::SpecializationMode::Disable)
            return false;
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
        if (mSpecialization == RuntimeOptions::SpecializationMode::Force || mContext.Options.Specialization == RuntimeOptions::SpecializationMode::Force)
            return true;
        else if (mSpecialization == RuntimeOptions::SpecializationMode::Disable || mContext.Options.Specialization == RuntimeOptions::SpecializationMode::Disable)
            return false;
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

std::string ShadingTree::acquireInteger(const std::string& prop_name, int number, const IntegerOptions& options)
{
    if (checkIfEmbed(number, options)) {
        return std::to_string(number);
    } else {
        const std::string id                     = currentClosureID() + "_" + LoaderUtils::escapeIdentifier(prop_name);
        mContext.LocalRegistry.IntParameters[id] = number;

        mHeaderLines.push_back("  let var_int_" + id + " = registry::get_local_parameter_i32(\"" + id + "\", 0);\n");
        return "var_int_" + id;
    }
}

std::string ShadingTree::acquireNumber(const std::string& prop_name, float number, const NumberOptions& options)
{
    if (checkIfEmbed(number, options)) {
        return std::to_string(number);
    } else {
        const std::string id                       = currentClosureID() + "_" + LoaderUtils::escapeIdentifier(prop_name);
        mContext.LocalRegistry.FloatParameters[id] = number;

        mHeaderLines.push_back("  let var_num_" + id + " = registry::get_local_parameter_f32(\"" + id + "\", 0);\n");
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

        mHeaderLines.push_back("  let var_color_" + id + " = registry::get_local_parameter_color(\"" + id + "\", color_builtins::black);\n");
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

        mHeaderLines.push_back("  let var_vec_" + id + " = registry::get_local_parameter_vec3(\"" + id + "\", vec3_expand(0));\n");
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