#include "Transpiler.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "ShadingTree.h"
#include "StringUtils.h"

#include "PExpr.h"

#include <algorithm>
#include <map>
#include <optional>
#include <unordered_map>

namespace IG {
using PExprType   = PExpr::ElementaryType;
using ParamArr    = std::vector<std::string>;
using TypeArr     = std::vector<PExprType>;
using MapFunction = std::function<std::string(size_t&, const ParamArr&)>;

inline std::string tex_name(const std::string& name)
{
    return "tex_" + name;
}

inline static std::string typeConstant(float f, PExprType arithType)
{
    switch (arithType) {
    default:
    case PExprType::Number:
        return std::to_string(f);
    case PExprType::Integer:
        return std::to_string((int)f);
    case PExprType::Vec2:
        return "vec2_expand(" + std::to_string(f) + ")";
    case PExprType::Vec3:
        return "vec3_expand(" + std::to_string(f) + ")";
    case PExprType::Vec4:
        return "vec4_expand(" + std::to_string(f) + ")";
    }
}

inline static constexpr std::string_view typeArtic(PExprType type)
{
    switch (type) {
    default:
    case PExprType::Boolean:
        return "bool";
    case PExprType::String:
        return "&[u8]";
    case PExprType::Number:
        return "f32";
    case PExprType::Integer:
        return "i32";
    case PExprType::Vec2:
        return "Vec2";
    case PExprType::Vec3:
        return "Vec3";
    case PExprType::Vec4:
        return "Vec4";
    }
}

inline static std::optional<std::string> extractConstantString(const std::string& arg)
{
    if (arg.size() < 2)
        return {};

    const char c = arg.front();
    if (c != '"' && c != '\'')
        return {};

    if (arg.back() != c)
        return {};

    return arg.substr(1, arg.size() - 2);
}

inline static std::string handleCheckRayFlag(size_t&, const ParamArr& args)
{
    const auto flag_name = extractConstantString(to_lowercase(args.front()));
    if (!flag_name.has_value()) {
        IG_LOG(L_ERROR) << "check_ray_flag expects a constant string as an argument but got " << args.front() << std::endl;
        return "false";
    }

    std::string flag;
    if (flag_name.value() == "camera") {
        flag = "ray_flag_camera";
    } else if (flag_name.value() == "light") {
        flag = "ray_flag_light";
    } else if (flag_name.value() == "bounce") {
        flag = "ray_flag_bounce";
    } else if (flag_name.value() == "shadow") {
        flag = "ray_flag_shadow";
    } else {
        IG_LOG(L_ERROR) << "Unknown check_ray_flag argument " << args.front() << std::endl;
        return "false";
    }

    return "check_ray_visibility(ctx.ray, " + flag + ")";
}

enum class TransformInterpretation {
    Point,
    Direction,
    Normal
};

inline static std::string handleTransform(TransformInterpretation interpretation, const std::string& value, const std::string& from, const std::string& to)
{
    const auto from_name = extractConstantString(to_lowercase(from));
    const auto to_name   = extractConstantString(to_lowercase(to));

    if (!from_name.has_value() || !to_name.has_value()) {
        IG_LOG(L_ERROR) << "transform expects two constant strings as additional arguments but got from "
                        << " to " << to << std::endl;
        return "false";
    }

    // If same space, no transformation required
    if (from_name.value() == to_name.value())
        return value;

    // TODO: Support more coordinate spaces
    bool object_to_global;
    if (from_name.value() == "object" && to_name.value() == "global") {
        object_to_global = true;
    } else if (from_name.value() == "global" && to_name.value() == "object") {
        object_to_global = false;
    } else {
        IG_LOG(L_ERROR) << "Unknown transform coordinate spaces given from " << from << " to " << to << std::endl;
        return "vec3_expand(0)";
    }

    // Get suffix
    std::string suffix;
    switch (interpretation) {
    case TransformInterpretation::Point:
        suffix = "point";
        break;
    case TransformInterpretation::Direction:
        suffix = "direction";
        break;
    case TransformInterpretation::Normal:
        suffix = "normal";
        break;
    }

    if (object_to_global) {
        return "ctx.coord.to_global_" + suffix + "(" + value + ")";
    } else {
        return "ctx.coord.to_local_" + suffix + "(" + value + ")";
    }
}

// We assume function state and return value to depend only on the parameters!
template <typename Func>
inline static std::string collapseFunction(size_t& uuid_counter, Func func, const std::vector<std::string>& args)
{
    constexpr size_t MinLength = 16; // At least a single argument has to larger than this number to generate a collapsed function

    if (args.size() <= 1)
        return func(args);

    size_t uuid_counter2 = uuid_counter;
    size_t max_length    = 0;
    std::unordered_map<std::string, std::string> set;
    for (const auto& arg : args) {
        max_length = std::max(max_length, arg.size());
        if (set.count(arg) == 0)
            set[arg] = "a" + std::to_string(uuid_counter2++);
    }

    if (max_length < MinLength || set.size() == args.size()) {
        return func(args);
    } else {
        uuid_counter = uuid_counter2;
        std::vector<std::string> new_args;
        new_args.reserve(args.size());
        for (const auto& arg : args)
            new_args.emplace_back(set.at(arg));

        std::stringstream stream;
        stream << "(@|| { ";

        for (const auto& p : set)
            stream << "let " << p.second << " = " << p.first << ";";

        stream << func(new_args)
               << "})()";
        return stream.str();
    }
}

static inline std::string handleSelect(size_t& uuidCounter, const ParamArr& args)
{
    if (args[1] == args[2])
        return args[1];
    else
        return collapseFunction(
            uuidCounter,
            [](const ParamArr& args2) { return "select(" + args2[0] + "," + args2[1] + "," + args2[2] + ")"; },
            args);
}

inline static std::string handleCurveLookup(size_t& uuidCounter, const ParamArr& initArgs)
{
    const auto interpolation = extractConstantString(to_lowercase(initArgs.front()));
    if (!interpolation.has_value()) {
        IG_LOG(L_ERROR) << "curve_lookup expects one constant string as initial argument but got " << initArgs.front() << std::endl;
        return "0";
    }

    bool linear;
    if (interpolation == "constant") {
        linear = false;
    } else if (interpolation == "linear") {
        linear = true;
    } else {
        IG_LOG(L_ERROR) << "Unknown curve_lookup argument " << initArgs.front() << std::endl;
        return "0";
    }

    // Remove first argument
    ParamArr newArgs = initArgs;
    newArgs.erase(newArgs.begin());

    return collapseFunction(
        uuidCounter,
        [linear](const std::vector<std::string>& args) {
            std::stringstream x_values;
            x_values << "@|i:i32| [";
            for (size_t i = 2; i < args.size(); ++i)
                x_values << "vec2_at(" << args.at(i) << ", 0), ";
            x_values << "](i)";

            std::stringstream y_values;
            y_values << "@|i:i32| [";
            for (size_t i = 2; i < args.size(); ++i)
                y_values << "vec2_at(" << args.at(i) << ", 1), ";
            y_values << "](i)";

            std::string interpolate = linear ? "true" : "false";
            std::string extrapolate = args.front();
            return "math::lookup_curve(" + std::to_string(args.size() - 2) + ", " + args.at(1) + ", " + x_values.str() + ", " + y_values.str() + ", " + interpolate + ", " + extrapolate + ")";
        },
        newArgs);
}

inline static std::string handleVoronoiGen(size_t& uuidCounter, bool colored, size_t dim, const ParamArr& initArgs)
{
    const size_t FeatureArg = 3;
    const size_t DistArg    = 4;

    IG_ASSERT(dim >= 1 && dim <= 3, "Only supporting voronoi for 1d, 2d and 3d");

    const auto feature_v = extractConstantString(to_lowercase(initArgs.at(FeatureArg)));
    if (!feature_v.has_value()) {
        IG_LOG(L_ERROR) << "voronoi expects a constant string for its feature argument but got " << initArgs.at(FeatureArg) << std::endl;
        return "0";
    }
    std::string feature = feature_v.value();

    if (feature == "f1") {
        feature = "VoronoiFeature::F1";
    } else if (feature == "f2") {
        feature = "VoronoiFeature::F2";
    } else {
        IG_LOG(L_WARNING) << "Unknown voronoi feature '" << feature << "'. Using 'f1' instead." << std::endl;
        feature = "VoronoiFeature::F1";
    }

    std::string dist;
    if (dim > 1) {
        const auto dist_v = extractConstantString(to_lowercase(initArgs.at(DistArg)));

        if (!dist_v.has_value()) {
            IG_LOG(L_ERROR) << "voronoi expects a constant string for its distance argument but got " << initArgs.at(DistArg) << std::endl;
            return "0";
        }

        dist = dist_v.value();

        if (dist == "manhatten") {
            dist = "VoronoiDistance::Manhatten";
        } else if (dist == "chebyshev") {
            dist = "VoronoiDistance::Chebyshev";
        } else if (dist == "minkowski") {
            std::string exponent = "4";
            if (initArgs.size() > 5)
                exponent = initArgs.at(DistArg + 1);
            dist = "VoronoiDistance::Minkowski(" + exponent + ")";
        } else if (dist == "euclidean") {
            dist = "VoronoiDistance::Euclidean";
        } else {
            IG_LOG(L_WARNING) << "Unknown voronoi distance function '" << dist << "'. Using 'euclidean' instead." << std::endl;
            dist = "VoronoiDistance::Euclidean";
        }
    }

    // Remove additional arguments
    ParamArr newArgs = initArgs;
    newArgs.resize(3);

    return collapseFunction(
        uuidCounter,
        [=](const std::vector<std::string>& args) {
            if (!colored) {
                if (dim == 1)
                    return "voronoi1_gen(" + args.at(0) + ", " + args.at(1) + ", " + args.at(2) + ", " + feature + ").0";
                else if (dim == 2)
                    return "voronoi2_gen(" + args.at(0) + ", " + args.at(1) + ", " + args.at(2) + ", " + feature + ", " + dist + ").0";
                else
                    return "voronoi3_gen(" + args.at(0) + ", " + args.at(1) + ", " + args.at(2) + ", " + feature + ", " + dist + ").0";
            } else {
                if (dim == 1)
                    return "color_to_vec4(voronoi1_gen(" + args.at(0) + ", " + args.at(1) + ", " + args.at(2) + ", " + feature + ").1)";
                else if (dim == 2)
                    return "color_to_vec4(voronoi2_gen(" + args.at(0) + ", " + args.at(1) + ", " + args.at(2) + ", " + feature + ", " + dist + ").1)";
                else
                    return "color_to_vec4(voronoi3_gen(" + args.at(0) + ", " + args.at(1) + ", " + args.at(2) + ", " + feature + ", " + dist + ").1)";
            }
        },
        newArgs);
}

static inline std::string_view dumpType(PExprType type)
{
    return PExpr::toString(type);
}

// Internal Variables
struct InternalVariable {
    std::string Map;
    PExprType Type;
    bool IsConstant;
};
static const std::unordered_map<std::string, InternalVariable> sInternalVariables = {
    { "uv", { "vec3_to_2(ctx.uvw)", PExprType::Vec2, false } },
    { "uvw", { "ctx.uvw", PExprType::Vec3, false } },
    { "prim_coords", { "ctx.surf.prim_coords", PExprType::Vec2, false } },
    { "P", { "ctx.surf.point", PExprType::Vec3, false } },
    { "Np", { "ctx.coord.to_normalized_point(ctx.surf.point)", PExprType::Vec3, false } },
    { "V", { "vec3_neg(ctx.ray.dir)", PExprType::Vec3, false } },
    { "Rd", { "vec3_neg(ctx.ray.dir)", PExprType::Vec3, false } },
    { "Ro", { "ctx.ray.org", PExprType::Vec3, false } },
    { "N", { "ctx.surf.local.col(2)", PExprType::Vec3, false } },
    { "Ng", { "ctx.surf.face_normal", PExprType::Vec3, false } },
    { "Nx", { "ctx.surf.local.col(0)", PExprType::Vec3, false } },
    { "Ny", { "ctx.surf.local.col(1)", PExprType::Vec3, false } },
    { "frontside", { "ctx.surf.is_entering", PExprType::Boolean, false } },
    { "entity_id", { "ctx.entity_id", PExprType::Integer, false } },
    { "Ix", { "ctx.pixel.x", PExprType::Integer, false } },
    { "Iy", { "ctx.pixel.y", PExprType::Integer, false } },
    { "t", { "ctx.info.time", PExprType::Number, false } },
    { "frame", { "ctx.info.frame", PExprType::Integer, false } },
    { "Pi", { "flt_pi", PExprType::Number, true } },
    { "E", { "flt_e", PExprType::Number, true } },
    { "Eps", { "flt_eps", PExprType::Number, true } },
    { "NumMax", { "flt_max", PExprType::Number, true } },
    { "NumMin", { "flt_min", PExprType::Number, true } },
    { "Inf", { "flt_inf", PExprType::Number, true } }
};

// Dyn Functions
static inline MapFunction createFunction(const std::string& func)
{
    return [func](size_t& uuid_counter, const ParamArr& args) {
        return collapseFunction(
            uuid_counter,
            [func](const ParamArr& args2) {
                std::stringstream stream;

                stream << func << "(";
                for (size_t i = 0; i < args2.size(); ++i) {
                    stream << args2.at(i);
                    if (i < args2.size() - 1)
                        stream << ", ";
                }
                stream << ")";
                return stream.str();
            },
            args);
    };
}

static inline MapFunction createColorFunctionIn1(const std::string& func)
{
    return [func](size_t&, const ParamArr& args) {
        IG_ASSERT(args.size() == 1, "Only one argument expected for createColorFunctionIn1");
        std::stringstream stream;
        stream << func << "(vec4_to_color(" << args[0] << "))";
        return stream.str();
    };
}

static inline MapFunction createColorFunctionInOut1(const std::string& func)
{
    return [func](size_t&, const ParamArr& args) {
        IG_ASSERT(args.size() == 1, "Only one argument expected for createColorFunctionInOut1");
        std::stringstream stream;
        stream << "color_to_vec4(" << func << "(vec4_to_color(" << args[0] << ")))";
        return stream.str();
    };
}

static inline MapFunction createColorFunctionOut(const std::string& func)
{
    return [func](size_t& uuid_counter, const ParamArr& args) {
        return collapseFunction(
            uuid_counter,
            [func](const ParamArr& args2) {
                std::stringstream stream;

                stream << "color_to_vec4(" << func << "(";
                for (size_t i = 0; i < args2.size(); ++i) {
                    stream << args2.at(i);
                    if (i < args2.size() - 1)
                        stream << ", ";
                }
                stream << "))";
                return stream.str();
            },
            args);
    };
}

static inline MapFunction createNto1MapFunction(const std::string& func, size_t dim)
{
    return [func, dim](size_t&, const ParamArr& args) {
        IG_ASSERT(args.size() == 1, "Only one argument expected for createNto1MapFunction");
        std::stringstream stream;

        if (dim > 1)
            stream << "vec" << dim << "_map(" << args[0] << ", @|x:f32| " << func << "(x))";
        else
            stream << func << "(" << args[0] << ")";
        return stream.str();
    };
}

static inline MapFunction createZipFunction(const std::string& func, size_t dim)
{
    return [func, dim](size_t& uuid_counter, const ParamArr& args) {
        IG_ASSERT(args.size() == 2, "Only one argument expected for createZipFunction");
        return collapseFunction(
            uuid_counter,
            [func, dim](const ParamArr& args2) {
                std::stringstream stream;

                if (dim > 1)
                    stream << "vec" << dim << "_zip(" << args2[0] << ", " << args2[1] << ", @|x:f32, y:f32| " << func << "(x, y))";
                else
                    stream << func << "(" << args2[0] << ", " << args2[1] << ")";
                return stream.str();
            },
            args);
    };
}

static inline MapFunction createZip3Function(const std::string& func, size_t dim)
{
    return [func, dim](size_t& uuid_counter, const ParamArr& args) {
        IG_ASSERT(args.size() == 3, "Only one argument expected for createZip3Function");
        return collapseFunction(
            uuid_counter,
            [func, dim](const ParamArr& args2) {
                std::stringstream stream;

                if (dim > 1)
                    stream << "vec" << dim << "_zip3(" << args2[0] << ", " << args2[1] << ", " << args2[2] << ", @|x:f32, y:f32, z:f32| " << func << "(x, y, z))";
                else
                    stream << func << "(" << args2[0] << ", " << args2[1] << ", " << args2[2] << ")";
                return stream.str();
            },
            args);
    };
}

static inline MapFunction createColorLerpFunction(const std::string& func)
{
    return [func](size_t& uuid_counter, const ParamArr& args) {
        IG_ASSERT(args.size() == 3, "Only one argument expected for createColorLerpFunction");
        return collapseFunction(
            uuid_counter,
            [func](const ParamArr& args2) {
                std::stringstream stream;

                stream << "color_to_vec4(" << func << "(vec4_to_color(" << args2[0] << "), vec4_to_color(" << args2[1] << "), " << args2[2] << "))";
                return stream.str();
            },
            args);
    };
}

struct FunctionDef {
    MapFunction Func;
    std::string Name;
    PExprType ReturnType;
    TypeArr Arguments;
    bool Variadic; // If true, the last argument type can be added infinitely times

    inline std::string signature() const
    {
        std::stringstream stream;
        stream << Name << "(";
        for (size_t i = 0; i < Arguments.size(); ++i) {
            stream << dumpType(Arguments.at(i));
            if (i < Arguments.size() - 1)
                stream << ", ";
            else if (Variadic)
                stream << ", ...";
        }
        stream << ") -> " << dumpType(ReturnType);
        return stream.str();
    }

    inline PExpr::FunctionDef definition() const
    {
        return PExpr::FunctionDef(Name, ReturnType, Arguments);
    }

    inline PExpr::FunctionDef definition(const TypeArr& arr) const
    {
        return PExpr::FunctionDef(Name, ReturnType, arr);
    }

    inline bool checkParameters(const TypeArr& paramTypes) const
    {
        if (Variadic && paramTypes.size() > 0) {
            if (Arguments.size() > paramTypes.size())
                return false;

            for (size_t i = 0; i < Arguments.size(); ++i) {
                if (paramTypes.at(i) != Arguments.at(i))
                    return false;
            }

            const auto lastType = Arguments.back();
            for (size_t i = Arguments.size(); i < paramTypes.size(); ++i) {
                if (paramTypes.at(i) != lastType)
                    return false;
            }
            return true;
        } else {
            return Arguments.size() == paramTypes.size() && std::equal(Arguments.begin(), Arguments.end(), paramTypes.begin());
        }
    }

    inline bool check(const PExpr::FunctionLookup& lkp, bool exact = false) const
    {
        if (lkp.name() != Name)
            return false;

        if (Variadic) {
            const auto check = [exact](PExprType a, PExprType b) {
                return exact ? (a == b) : (PExpr::isConvertible(a, b));
            };

            if (Arguments.size() > lkp.parameters().size())
                return false;

            for (size_t i = 0; i < Arguments.size(); ++i) {
                if (!check(lkp.parameters().at(i), Arguments.at(i)))
                    return false;
            }

            const auto lastType = Arguments.back();
            for (size_t i = Arguments.size(); i < lkp.parameters().size(); ++i) {
                if (!check(lkp.parameters().at(i), lastType))
                    return false;
            }
            return true;
        } else {
            return Arguments.size() == lkp.parameters().size() && lkp.matchParameter(Arguments, exact);
        }
    }

    inline std::optional<PExpr::FunctionDef> matchDef(const PExpr::FunctionLookup& lkp, bool exact = false) const
    {
        if (check(lkp, exact)) {
            if (!Variadic) {
                return definition();
            } else {
                TypeArr args = Arguments;
                for (size_t i = Arguments.size(); i < lkp.parameters().size(); ++i)
                    args.push_back(Arguments.back());
                return definition(args);
            }
        } else {
            return {};
        }
    }

    inline std::string call(size_t& uuidCounter, const ParamArr& args) const
    {
        return Func(uuidCounter, args);
    }
};

template <typename... Args>
inline static FunctionDef constructFunctionDef(const std::string& name, MapFunction func, bool variadic, PExprType retType, const Args&... args)
{
    return FunctionDef{
        func,
        name,
        retType,
        TypeArr{ args... },
        variadic
    };
}

// Defs
template <typename... Args>
static inline std::pair<std::string, FunctionDef> cF(const std::string& name, MapFunction func, PExprType retType, const Args&... args)
{
    return std::make_pair(name, constructFunctionDef(name, func, false, retType, args...));
}

template <typename... Args>
static inline std::pair<std::string, FunctionDef> cFV(const std::string& name, MapFunction func, PExprType retType, const Args&... args)
{
    return std::make_pair(name, constructFunctionDef(name, func, true, retType, args...));
}

#define _MF1A(name, iname)                                                           \
    cF(name, createNto1MapFunction(iname, 1), PExprType::Number, PExprType::Number), \
        cF(name, createNto1MapFunction(iname, 2), PExprType::Vec2, PExprType::Vec2), \
        cF(name, createNto1MapFunction(iname, 3), PExprType::Vec3, PExprType::Vec3), \
        cF(name, createNto1MapFunction(iname, 4), PExprType::Vec4, PExprType::Vec4)

#define _MF2A(name, iname)                                                                          \
    cF(name, createZipFunction(iname, 1), PExprType::Number, PExprType::Number, PExprType::Number), \
        cF(name, createZipFunction(iname, 2), PExprType::Vec2, PExprType::Vec2, PExprType::Vec2),   \
        cF(name, createZipFunction(iname, 3), PExprType::Vec3, PExprType::Vec3, PExprType::Vec3),   \
        cF(name, createZipFunction(iname, 4), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4)

#define _MF3A(name, iname)                                                                                              \
    cF(name, createZip3Function(iname, 1), PExprType::Number, PExprType::Number, PExprType::Number, PExprType::Number), \
        cF(name, createZip3Function(iname, 2), PExprType::Vec2, PExprType::Vec2, PExprType::Vec2, PExprType::Vec2),     \
        cF(name, createZip3Function(iname, 3), PExprType::Vec3, PExprType::Vec3, PExprType::Vec3, PExprType::Vec3),     \
        cF(name, createZip3Function(iname, 4), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Vec4)

static const std::multimap<std::string, FunctionDef> sInternalFunctions = {
    _MF1A("sin", "math_builtins::sin"),
    _MF1A("cos", "math_builtins::cos"),
    _MF1A("tan", "math_builtins::tan"),
    _MF1A("asin", "math_builtins::asin"),
    _MF1A("acos", "math_builtins::acos"),
    _MF1A("atan", "math_builtins::atan"),
    _MF1A("exp", "math_builtins::exp"),
    _MF1A("exp2", "math_builtins::exp2"),
    _MF1A("log", "math_builtins::log"),
    _MF1A("log2", "math_builtins::log2"),
    _MF1A("log10", "math_builtins::log10"),
    _MF1A("floor", "math_builtins::floor"),
    _MF1A("ceil", "math_builtins::ceil"),
    _MF1A("round", "math_builtins::round"),
    _MF1A("fract", "math::fract"),
    _MF1A("trunc", "math::trunc"),
    _MF1A("sqrt", "math_builtins::sqrt"),
    _MF1A("cbrt", "math_builtins::cbrt"),
    _MF1A("abs", "math_builtins::fabs"),
    cF("abs", createFunction("abs"), PExprType::Integer, PExprType::Integer),
    _MF1A("sign", "math::signf"),
    cF("sign", createFunction("math::sign"), PExprType::Integer, PExprType::Integer),
    cF("signbit", createFunction("math_builtins::signbit"), PExprType::Boolean, PExprType::Integer),
    cF("signbit", createFunction("math_builtins::signbit"), PExprType::Boolean, PExprType::Number),

    _MF1A("rad", "rad"),
    _MF1A("deg", "deg"),

    cF("hash", createFunction("hash_rndf"), PExprType::Number, PExprType::Number),
    cF("smoothstep", createFunction("smoothstep"), PExprType::Number, PExprType::Number),
    cF("smootherstep", createFunction("smootherstep"), PExprType::Number, PExprType::Number),
    cF("rgbtoxyz", createColorFunctionInOut1("srgb_to_xyz"), PExprType::Vec4, PExprType::Vec4),
    cF("xyztorgb", createColorFunctionInOut1("xyz_to_srgb"), PExprType::Vec4, PExprType::Vec4),
    cF("rgbtohsv", createColorFunctionInOut1("srgb_to_hsv"), PExprType::Vec4, PExprType::Vec4),
    cF("hsvtorgb", createColorFunctionInOut1("hsv_to_srgb"), PExprType::Vec4, PExprType::Vec4),
    cF("rgbtohsl", createColorFunctionInOut1("srgb_to_hsl"), PExprType::Vec4, PExprType::Vec4),
    cF("hsltorgb", createColorFunctionInOut1("hsl_to_srgb"), PExprType::Vec4, PExprType::Vec4),
    cF("norm", createFunction("vec2_normalize"), PExprType::Vec2, PExprType::Vec2),
    cF("norm", createFunction("vec3_normalize"), PExprType::Vec3, PExprType::Vec3),
    cF("norm", createFunction("vec4_normalize"), PExprType::Vec4, PExprType::Vec4),
    cF("length", createFunction("vec2_len"), PExprType::Number, PExprType::Vec2),
    cF("length", createFunction("vec3_len"), PExprType::Number, PExprType::Vec3),
    cF("length", createFunction("vec4_len"), PExprType::Number, PExprType::Vec4),
    cF("sum", createFunction("vec2_sum"), PExprType::Number, PExprType::Vec2),
    cF("sum", createFunction("vec3_sum"), PExprType::Number, PExprType::Vec3),
    cF("sum", createFunction("vec4_sum"), PExprType::Number, PExprType::Vec4),
    cF("avg", createFunction("vec2_average"), PExprType::Number, PExprType::Vec2),
    cF("avg", createFunction("vec3_average"), PExprType::Number, PExprType::Vec3),
    cF("avg", createFunction("vec4_average"), PExprType::Number, PExprType::Vec4),
    cF("luminance", createColorFunctionIn1("color_luminance"), PExprType::Number, PExprType::Vec4),
    cF("blackbody", createColorFunctionOut("math::blackbody"), PExprType::Vec4, PExprType::Number),

    cF("checkerboard", createFunction("node_checkerboard2"), PExprType::Integer, PExprType::Vec2),
    cF("checkerboard", createFunction("node_checkerboard3"), PExprType::Integer, PExprType::Vec3),

    _MF2A("min", "math_builtins::fmin"),
    cF("min", createFunction("min"), PExprType::Integer, PExprType::Integer, PExprType::Integer),
    _MF2A("max", "math_builtins::fmax"),
    cF("max", createFunction("max"), PExprType::Integer, PExprType::Integer, PExprType::Integer),

    _MF2A("snap", "math::snap"),
    _MF2A("pingpong", "math::pingpong"),
    _MF2A("pow", "math_builtins::pow"),
    _MF2A("atan2", "math_builtins::atan2"),
    _MF2A("fmod", "math::fmod"),

    cF("angle", createFunction("vec2_angle"), PExprType::Number, PExprType::Vec2, PExprType::Vec2),
    cF("angle", createFunction("vec3_angle"), PExprType::Number, PExprType::Vec3, PExprType::Vec3),
    cF("angle", createFunction("vec4_angle"), PExprType::Number, PExprType::Vec4, PExprType::Vec4),
    cF("dot", createFunction("vec2_dot"), PExprType::Number, PExprType::Vec2, PExprType::Vec2),
    cF("dot", createFunction("vec3_dot"), PExprType::Number, PExprType::Vec3, PExprType::Vec3),
    cF("dot", createFunction("vec4_dot"), PExprType::Number, PExprType::Vec4, PExprType::Vec4),
    cF("dist", createFunction("vec2_dist"), PExprType::Number, PExprType::Vec2, PExprType::Vec2),
    cF("dist", createFunction("vec3_dist"), PExprType::Number, PExprType::Vec3, PExprType::Vec3),
    cF("dist", createFunction("vec4_dist"), PExprType::Number, PExprType::Vec4, PExprType::Vec4),

    cF("cross", createFunction("vec3_cross"), PExprType::Vec3, PExprType::Vec3, PExprType::Vec3),
    cF("reflect", createFunction("vec3_reflect"), PExprType::Vec3, PExprType::Vec3, PExprType::Vec3),
    cF("rotate_euler", createFunction("vec3_rotate_euler"), PExprType::Vec3, PExprType::Vec3, PExprType::Vec3),
    cF("rotate_euler_inverse", createFunction("vec3_rotate_euler_inverse"), PExprType::Vec3, PExprType::Vec3, PExprType::Vec3),
    cF("fresnel_dielectric", createFunction("math::fresnel_dielectric"), PExprType::Number, PExprType::Number, PExprType::Number),

    // TODO: refract (V, N, ior) -> Vec3 !

    cF("noise", createFunction("noise1_def"), PExprType::Number, PExprType::Number),
    cF("noise", createFunction("noise1"), PExprType::Number, PExprType::Number, PExprType::Number),
    cF("noise", createFunction("noise2_def"), PExprType::Number, PExprType::Vec2),
    cF("noise", createFunction("noise2_v"), PExprType::Number, PExprType::Vec2, PExprType::Number),
    cF("noise", createFunction("noise3_def"), PExprType::Number, PExprType::Vec3),
    cF("noise", createFunction("noise3_v"), PExprType::Number, PExprType::Vec3, PExprType::Number),
    cF("snoise", createFunction("snoise1_def"), PExprType::Number, PExprType::Number),
    cF("snoise", createFunction("snoise1"), PExprType::Number, PExprType::Number, PExprType::Number),
    cF("snoise", createFunction("snoise2_def"), PExprType::Number, PExprType::Vec2),
    cF("snoise", createFunction("snoise2"), PExprType::Number, PExprType::Vec2, PExprType::Number),
    cF("snoise", createFunction("snoise3_def"), PExprType::Number, PExprType::Vec3),
    cF("snoise", createFunction("snoise3"), PExprType::Number, PExprType::Vec3, PExprType::Number),
    cF("pnoise", createFunction("pnoise1_def"), PExprType::Number, PExprType::Number),
    cF("pnoise", createFunction("pnoise1"), PExprType::Number, PExprType::Number, PExprType::Number),
    cF("pnoise", createFunction("pnoise2_def"), PExprType::Number, PExprType::Vec2),
    cF("pnoise", createFunction("pnoise2"), PExprType::Number, PExprType::Vec2, PExprType::Number),
    cF("pnoise", createFunction("pnoise3_def"), PExprType::Number, PExprType::Vec3),
    cF("pnoise", createFunction("pnoise3"), PExprType::Number, PExprType::Vec3, PExprType::Number),
    cF("cellnoise", createFunction("cellnoise1_def"), PExprType::Number, PExprType::Number),
    cF("cellnoise", createFunction("cellnoise1"), PExprType::Number, PExprType::Number, PExprType::Number),
    cF("cellnoise", createFunction("cellnoise2_def"), PExprType::Number, PExprType::Vec2),
    cF("cellnoise", createFunction("cellnoise2"), PExprType::Number, PExprType::Vec2, PExprType::Number),
    cF("cellnoise", createFunction("cellnoise3_def"), PExprType::Number, PExprType::Vec3),
    cF("cellnoise", createFunction("cellnoise3"), PExprType::Number, PExprType::Vec3, PExprType::Number),
    cF("perlin", createFunction("perlin2_def"), PExprType::Number, PExprType::Vec2),
    cF("perlin", createFunction("perlin2"), PExprType::Number, PExprType::Vec2, PExprType::Number),
    cF("sperlin", createFunction("sperlin2_def"), PExprType::Number, PExprType::Vec2),
    cF("sperlin", createFunction("sperlin2"), PExprType::Number, PExprType::Vec2, PExprType::Number),

    cF("voronoi", createFunction("voronoi1_def"), PExprType::Number, PExprType::Number),
    cF("voronoi", createFunction("voronoi1"), PExprType::Number, PExprType::Number, PExprType::Number),
    cF(
        "voronoi",
        [](size_t& uuid, const ParamArr& args) { return handleVoronoiGen(uuid, false, 1, args); },
        PExprType::Number, PExprType::Number, PExprType::Number, PExprType::Number, PExprType::String),
    cF("voronoi", createFunction("voronoi2_def"), PExprType::Number, PExprType::Vec2),
    cF("voronoi", createFunction("voronoi2"), PExprType::Number, PExprType::Vec2, PExprType::Number),
    cF(
        "voronoi",
        [](size_t& uuid, const ParamArr& args) { return handleVoronoiGen(uuid, false, 2, args); },
        PExprType::Number, PExprType::Vec2, PExprType::Number, PExprType::Number, PExprType::String, PExprType::String),
    cF(
        "voronoi",
        [](size_t& uuid, const ParamArr& args) { return handleVoronoiGen(uuid, false, 2, args); },
        PExprType::Number, PExprType::Vec2, PExprType::Number, PExprType::Number, PExprType::String, PExprType::String, PExprType::Number),
    cF("voronoi", createFunction("voronoi3_def"), PExprType::Number, PExprType::Vec3),
    cF("voronoi", createFunction("voronoi3"), PExprType::Number, PExprType::Vec3, PExprType::Number),
    cF(
        "voronoi",
        [](size_t& uuid, const ParamArr& args) { return handleVoronoiGen(uuid, false, 3, args); },
        PExprType::Number, PExprType::Vec3, PExprType::Number, PExprType::Number, PExprType::String, PExprType::String),
    cF(
        "voronoi",
        [](size_t& uuid, const ParamArr& args) { return handleVoronoiGen(uuid, false, 3, args); },
        PExprType::Number, PExprType::Vec3, PExprType::Number, PExprType::Number, PExprType::String, PExprType::String, PExprType::Number),

    cF("fbm", createFunction("fbm2_def"), PExprType::Number, PExprType::Vec2),
    cF("fbm", createFunction("fbm2"), PExprType::Number, PExprType::Vec2, PExprType::Number),
    cF("fbm", createFunction("fbm2_arg"), PExprType::Number, PExprType::Vec2, PExprType::Number, PExprType::Integer, PExprType::Number, PExprType::Number),
    cF("gabor", createFunction("gabor2_def"), PExprType::Number, PExprType::Vec2),
    cF("gabor", createFunction("gabor2"), PExprType::Number, PExprType::Vec2, PExprType::Number),
    cF("gabor", createFunction("gabor2_gen"), PExprType::Number, PExprType::Vec2, PExprType::Number, PExprType::Integer, PExprType::Number, PExprType::Number, PExprType::Number),

    cF("cnoise", createColorFunctionOut("cnoise1_def"), PExprType::Vec4, PExprType::Number),
    cF("cnoise", createColorFunctionOut("cnoise1"), PExprType::Vec4, PExprType::Number, PExprType::Number),
    cF("cnoise", createColorFunctionOut("cnoise2_def"), PExprType::Vec4, PExprType::Vec2),
    cF("cnoise", createColorFunctionOut("cnoise2"), PExprType::Vec4, PExprType::Vec2, PExprType::Number),
    cF("cnoise", createColorFunctionOut("cnoise3_def"), PExprType::Vec4, PExprType::Vec3),
    cF("cnoise", createColorFunctionOut("cnoise3"), PExprType::Vec4, PExprType::Vec3, PExprType::Number),
    cF("cpnoise", createColorFunctionOut("cpnoise1_def"), PExprType::Vec4, PExprType::Number),
    cF("cpnoise", createColorFunctionOut("cpnoise1"), PExprType::Vec4, PExprType::Number, PExprType::Number),
    cF("cpnoise", createColorFunctionOut("cpnoise2_def"), PExprType::Vec4, PExprType::Vec2),
    cF("cpnoise", createColorFunctionOut("cpnoise2"), PExprType::Vec4, PExprType::Vec2, PExprType::Number),
    cF("cpnoise", createColorFunctionOut("cpnoise3_def"), PExprType::Vec4, PExprType::Vec3),
    cF("cpnoise", createColorFunctionOut("cpnoise3"), PExprType::Vec4, PExprType::Vec3, PExprType::Number),
    cF("ccellnoise", createColorFunctionOut("ccellnoise1_def"), PExprType::Vec4, PExprType::Number),
    cF("ccellnoise", createColorFunctionOut("ccellnoise1"), PExprType::Vec4, PExprType::Number, PExprType::Number),
    cF("ccellnoise", createColorFunctionOut("ccellnoise2_def"), PExprType::Vec4, PExprType::Vec2),
    cF("ccellnoise", createColorFunctionOut("ccellnoise2"), PExprType::Vec4, PExprType::Vec2, PExprType::Number),
    cF("ccellnoise", createColorFunctionOut("ccellnoise3_def"), PExprType::Vec4, PExprType::Vec3),
    cF("ccellnoise", createColorFunctionOut("ccellnoise3"), PExprType::Vec4, PExprType::Vec3, PExprType::Number),
    cF("cperlin", createColorFunctionOut("cperlin2_def"), PExprType::Vec4, PExprType::Vec2),
    cF("cperlin", createColorFunctionOut("cperlin2"), PExprType::Vec4, PExprType::Vec2, PExprType::Number),

    cF("cvoronoi", createColorFunctionOut("cvoronoi1_def"), PExprType::Vec4, PExprType::Number),
    cF("cvoronoi", createColorFunctionOut("cvoronoi1"), PExprType::Vec4, PExprType::Number, PExprType::Number),
    cF(
        "cvoronoi",
        [](size_t& uuid, const ParamArr& args) { return handleVoronoiGen(uuid, true, 1, args); },
        PExprType::Vec4, PExprType::Number, PExprType::Number, PExprType::Number, PExprType::String),
    cF("cvoronoi", createColorFunctionOut("cvoronoi2_def"), PExprType::Vec4, PExprType::Vec2),
    cF("cvoronoi", createColorFunctionOut("cvoronoi2"), PExprType::Vec4, PExprType::Vec2, PExprType::Number),
    cF(
        "cvoronoi",
        [](size_t& uuid, const ParamArr& args) { return handleVoronoiGen(uuid, true, 2, args); },
        PExprType::Vec4, PExprType::Vec2, PExprType::Number, PExprType::Number, PExprType::String, PExprType::String),
    cF(
        "cvoronoi",
        [](size_t& uuid, const ParamArr& args) { return handleVoronoiGen(uuid, true, 2, args); },
        PExprType::Vec4, PExprType::Vec2, PExprType::Number, PExprType::Number, PExprType::String, PExprType::String, PExprType::Number),
    cF("cvoronoi", createColorFunctionOut("cvoronoi3_def"), PExprType::Vec4, PExprType::Vec3),
    cF("cvoronoi", createColorFunctionOut("cvoronoi3"), PExprType::Vec4, PExprType::Vec3, PExprType::Number),
    cF(
        "cvoronoi",
        [](size_t& uuid, const ParamArr& args) { return handleVoronoiGen(uuid, true, 3, args); },
        PExprType::Vec4, PExprType::Vec3, PExprType::Number, PExprType::Number, PExprType::String, PExprType::String),
    cF(
        "cvoronoi",
        [](size_t& uuid, const ParamArr& args) { return handleVoronoiGen(uuid, true, 3, args); },
        PExprType::Vec4, PExprType::Vec3, PExprType::Number, PExprType::Number, PExprType::String, PExprType::String, PExprType::Number),

    cF("cfbm", createColorFunctionOut("cfbm2_def"), PExprType::Vec4, PExprType::Vec2),
    cF("cfbm", createColorFunctionOut("cfbm2"), PExprType::Vec4, PExprType::Vec2, PExprType::Number),
    cF("cfbm", createColorFunctionOut("cfbm2_arg"), PExprType::Vec4, PExprType::Vec2, PExprType::Number, PExprType::Integer, PExprType::Number, PExprType::Number),

    _MF3A("clamp", "clampf"),
    cF("clamp", createFunction("clamp"), PExprType::Integer, PExprType::Integer, PExprType::Integer, PExprType::Integer),
    _MF3A("smin", "math::smoothmin"),
    _MF3A("smax", "math::smoothmax"),
    _MF3A("wrap", "math::wrap"),
    cF("fresnel_conductor", createFunction("math::fresnel_conductor"), PExprType::Number, PExprType::Number, PExprType::Number, PExprType::Number),

    cF("rotate_axis", createFunction("vec3_rotate_axis"), PExprType::Vec3, PExprType::Vec3, PExprType::Number, PExprType::Vec3),
    cF(
        "transform_point",
        [](size_t&, const ParamArr& args) { return handleTransform(TransformInterpretation::Point, args[0], args[1], args[2]); },
        PExprType::Vec3, PExprType::Vec3, PExprType::String, PExprType::String),
    cF(
        "transform_direction",
        [](size_t&, const ParamArr& args) { return handleTransform(TransformInterpretation::Direction, args[0], args[1], args[2]); },
        PExprType::Vec3, PExprType::Vec3, PExprType::String, PExprType::String),
    cF(
        "transform_normal",
        [](size_t&, const ParamArr& args) { return handleTransform(TransformInterpretation::Normal, args[0], args[1], args[2]); },
        PExprType::Vec3, PExprType::Vec3, PExprType::String, PExprType::String),

    cF("mix", createFunction("lerp"), PExprType::Number, PExprType::Number, PExprType::Number, PExprType::Number),
    cF("mix", createFunction("vec2_lerp"), PExprType::Vec2, PExprType::Vec2, PExprType::Vec2, PExprType::Number),
    cF("mix", createFunction("vec3_lerp"), PExprType::Vec3, PExprType::Vec3, PExprType::Vec3, PExprType::Number),
    cF("mix", createFunction("vec4_lerp"), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Number),
    cF("mix_screen", createColorLerpFunction("color_mix_screen"), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Number),
    cF("mix_overlay", createColorLerpFunction("color_mix_overlay"), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Number),
    cF("mix_dodge", createColorLerpFunction("color_mix_dodge"), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Number),
    cF("mix_burn", createColorLerpFunction("color_mix_burn"), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Number),
    cF("mix_soft", createColorLerpFunction("color_mix_soft"), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Number),
    cF("mix_linear", createColorLerpFunction("color_mix_linear"), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Number),
    cF("mix_hue", createColorLerpFunction("color_mix_hue"), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Number),
    cF("mix_saturation", createColorLerpFunction("color_mix_saturation"), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Number),
    cF("mix_value", createColorLerpFunction("color_mix_value"), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Number),
    cF("mix_color", createColorLerpFunction("color_mix_color"), PExprType::Vec4, PExprType::Vec4, PExprType::Vec4, PExprType::Number),

    cF("vec2", createFunction("make_vec2"), PExprType::Vec2, PExprType::Number, PExprType::Number),
    cF("vec2", createFunction("vec2_expand"), PExprType::Vec2, PExprType::Number),
    cF("vec3", createFunction("make_vec3"), PExprType::Vec3, PExprType::Number, PExprType::Number, PExprType::Number),
    cF("vec3", createFunction("vec3_expand"), PExprType::Vec3, PExprType::Number),
    cF("vec4", createFunction("make_vec4"), PExprType::Vec4, PExprType::Number, PExprType::Number, PExprType::Number, PExprType::Number),
    cF("vec4", createFunction("vec4_expand"), PExprType::Vec4, PExprType::Number),
    cF("color", createFunction("make_vec4"), PExprType::Vec4, PExprType::Number, PExprType::Number, PExprType::Number, PExprType::Number),
    cF(
        "color",
        [](size_t& uuidCounter, const ParamArr& args) {
            return collapseFunction(
                uuidCounter, [](const ParamArr& args2) {
                    return "make_vec4(" + args2[0] + "," + args2[1] + "," + args2[2] + ", 1)";
                },
                args);
        },
        PExprType::Vec4, PExprType::Number, PExprType::Number, PExprType::Number),
    cF("color", createFunction("vec4_expand"), PExprType::Vec4, PExprType::Number),

    cF("check_ray_flag", handleCheckRayFlag, PExprType::Boolean, PExprType::String),

    cF("select", handleSelect, PExprType::Boolean, PExprType::Boolean, PExprType::Boolean, PExprType::Boolean),
    cF("select", handleSelect, PExprType::Integer, PExprType::Boolean, PExprType::Integer, PExprType::Integer),
    cF("select", handleSelect, PExprType::Number, PExprType::Boolean, PExprType::Number, PExprType::Number),
    cF("select", handleSelect, PExprType::Vec2, PExprType::Boolean, PExprType::Vec2, PExprType::Vec2),
    cF("select", handleSelect, PExprType::Vec3, PExprType::Boolean, PExprType::Vec3, PExprType::Vec3),
    cF("select", handleSelect, PExprType::Vec4, PExprType::Boolean, PExprType::Vec4, PExprType::Vec4),
    cF("select", handleSelect, PExprType::String, PExprType::Boolean, PExprType::String, PExprType::String),

    // Some special purpose functions
    cF("bump", createFunction("node_bump"), PExprType::Vec3, PExprType::Vec3, PExprType::Vec3, PExprType::Vec3, PExprType::Number, PExprType::Number, PExprType::Number),
    cF("ensure_valid_reflection", createFunction("ensure_valid_reflection"), PExprType::Vec3, PExprType::Vec3, PExprType::Vec3, PExprType::Vec3),

    cFV("lookup", handleCurveLookup, PExprType::Number, PExprType::String, PExprType::Boolean, PExprType::Number, PExprType::Vec2)
};
#undef _MF1A
#undef _MF2A
#undef _MF3A

// Other stuff
inline static std::string binaryCwise(const std::string& A, const std::string& B, PExprType arithType, const std::string& op, const std::string& func)
{
    switch (arithType) {
    default:
        return "(" + A + ") " + op + " (" + B + ")";
    case PExprType::Vec2:
        return "vec2_" + func + "(" + A + ", " + B + ")";
    case PExprType::Vec3:
        return "vec3_" + func + "(" + A + ", " + B + ")";
    case PExprType::Vec4:
        return "vec4_" + func + "(" + A + ", " + B + ")";
    }
}

class ArticVisitor : public PExpr::TranspileVisitor<std::string> {
private:
    std::unordered_set<std::string> mUsedTextures;  // Will count used textures
    std::unordered_set<std::string> mUsedVariables; // Will count used internal variables, but not constants
    size_t mUUIDCounter;
    const Transpiler* mParent;

public:
    inline explicit ArticVisitor(const Transpiler* parent)
        : mUUIDCounter(0)
        , mParent(parent)
    {
    }

    inline std::unordered_set<std::string>& usedTextures() { return mUsedTextures; }
    inline std::unordered_set<std::string>& usedVariables() { return mUsedVariables; }

    std::string onVariable(const std::string& name, PExprType expectedType) override
    {
        // Return internal variables if matched
        if (auto var = sInternalVariables.find(name); var != sInternalVariables.end() && var->second.Type == expectedType) {
            if (!var->second.IsConstant)
                mUsedVariables.insert(var->first);
            return var->second.Map;
        }

        // Return custom variables if matched
        if (auto var = mParent->mCustomVariableBool.find(name); expectedType == PExprType::Boolean && var != mParent->mCustomVariableBool.end()) {
            mUsedVariables.insert(var->first);
            return var->second;
        }
        if (auto var = mParent->mCustomVariableNumber.find(name); expectedType == PExprType::Number && var != mParent->mCustomVariableNumber.end()) {
            mUsedVariables.insert(var->first);
            return var->second;
        }
        if (auto var = mParent->mCustomVariableVector.find(name); expectedType == PExprType::Vec3 && var != mParent->mCustomVariableVector.end()) {
            mUsedVariables.insert(var->first);
            return var->second;
        }
        if (auto var = mParent->mCustomVariableColor.find(name); expectedType == PExprType::Vec4 && var != mParent->mCustomVariableColor.end()) {
            mUsedVariables.insert(var->first);
            return "color_to_vec4(" + var->second + ")";
        }

        IG_ASSERT(expectedType == PExprType::Vec4 && mParent->mTree.context().Options.Scene->texture(name) != nullptr, "Expected a valid texture name");
        mUsedTextures.insert(name);
        return "color_to_vec4(" + tex_name(mParent->mTree.getClosureID(name)) + "(ctx))";
    }

    std::string onInteger(PExpr::Integer v) override { return std::to_string(v) + ":i32"; }
    std::string onNumber(PExpr::Number v) override { return std::to_string(v) + ":f32"; }
    std::string onBool(bool v) override { return v ? "true" : "false"; }
    std::string onString(const std::string& v) override { return "\"" + v + "\""; }

    /// Implicit casts. Currently only int -> num, num -> int
    std::string onCast(const std::string& v, PExprType fromType, PExprType toType) override
    {
        if (fromType == PExprType::Integer && toType == PExprType::Number) {
            return "((" + v + ") as f32)";
        } else if (fromType == PExprType::Number && toType == PExprType::Integer) {
            return "((" + v + ") as i32)";
        } else {
            IG_ASSERT(false, "Only cast from int to num or back supported");
            return "0";
        }
    }

    /// +a, -a. Only called for arithmetic types
    std::string onPosNeg(bool isNeg, PExprType arithType, const std::string& v) override
    {
        if (!isNeg)
            return v;

        switch (arithType) {
        default:
            return "(-(" + v + "))";
        case PExprType::Vec2:
            return "vec2_neg(" + v + ")";
        case PExprType::Vec3:
            return "vec3_neg(" + v + ")";
        case PExprType::Vec4:
            return "vec4_neg(" + v + ")";
        }
    }

    // !a. Only called for bool
    std::string onNot(const std::string& v) override
    {
        return "!" + v;
    }

    /// a+b, a-b. Only called for arithmetic types. Both types are the same! Vectorized types should apply component wise
    std::string onAddSub(bool isSub, PExprType arithType, const std::string& a, const std::string& b) override
    {
        if (isSub) {
            if (a == b)
                return typeConstant(0, arithType);
            else
                return binaryCwise(a, b, arithType, "-", "sub");
        } else {
            if (a == b)
                return binaryCwise(a, typeConstant(2, arithType), arithType, "*", "mul");
            else
                return binaryCwise(a, b, arithType, "+", "add");
        }
    }

    /// a*b, a/b. Only called for arithmetic types. Both types are the same! Vectorized types should apply component wise
    std::string onMulDiv(bool isDiv, PExprType arithType, const std::string& a, const std::string& b) override
    {
        if (isDiv) {
            if (a == b) // Ignoring 0/0 here!
                return typeConstant(1, arithType);
            else
                return binaryCwise(a, b, arithType, "/", "div");
        } else {
            if (a == b)
                return onPow(arithType, a, typeConstant(2, PExprType::Number));
            else
                return binaryCwise(a, b, arithType, "*", "mul");
        }
    }

    /// a*f, f*a, a/f. A is an arithmetic type, f is 'num', except when a is 'int' then f is 'int' as well. Order of a*f or f*a does not matter
    std::string onScale(bool isDiv, PExprType aType, const std::string& a, const std::string& f) override
    {
        if (isDiv)
            return binaryCwise(a, f, aType, "/", "divf");
        else
            return binaryCwise(a, f, aType, "*", "mulf");
    }

    /// a^f A is an arithmetic type, f is 'num', except when a is 'int' then f is 'int' as well. Vectorized types should apply component wise
    std::string onPow(PExprType aType, const std::string& a, const std::string& f) override
    {
        if (aType == PExprType::Integer)
            return "(math_builtins::pow((" + a + ") as f32, (" + f + ") as f32) as i32)";
        else if (aType == PExprType::Number)
            return "math_builtins::pow(" + a + ", " + f + ")";
        else
            return binaryCwise(a, f, aType, "", "powf");
    }

    // a % b. Only called for int
    std::string onMod(const std::string& a, const std::string& b) override
    {
        return "((" + a + ") % (" + b + "))";
    }

    // a&&b, a||b. Only called for bool types
    std::string onAndOr(bool isOr, const std::string& a, const std::string& b) override
    {
        if (a == b)
            return a;

        if (isOr)
            return "((" + a + ") || (" + b + "))";
        else
            return "((" + a + ") && (" + b + "))";
    }

    /// a < b... Boolean operation. a & b are of the same type. Only called for scalar arithmetic types (int, num)
    std::string onRelOp(PExpr::RelationalOp op, PExprType, const std::string& a, const std::string& b) override
    {
        switch (op) {
        case PExpr::RelationalOp::Less:
            return "((" + a + ") < (" + b + "))";
        case PExpr::RelationalOp::Greater:
            return "((" + a + ") > (" + b + "))";
        case PExpr::RelationalOp::LessEqual:
            return "((" + a + ") <= (" + b + "))";
        case PExpr::RelationalOp::GreaterEqual:
            return "((" + a + ") >= (" + b + "))";
        default:
            IG_ASSERT(false, "Should not reach this point");
            return {};
        }
    }

    /// a==b, a!=b. For vectorized types it should check that all equal componont wise. The negation a!=b should behave as !(a==b)
    std::string onEqual(bool isNeg, PExprType type, const std::string& a, const std::string& b) override
    {
        std::string res = binaryCwise(a, b, type, "==", "eq");
        return isNeg ? "!" + res : res;
    }

    /// name(...). Call to a function. Necessary casts are already handled.
    std::string onFunctionCall(const std::string& name,
                               PExprType, const std::vector<PExprType>& argumentTypes,
                               const std::vector<std::string>& argumentPayloads) override
    {
        const auto range = sInternalFunctions.equal_range(name);
        if (range.first != range.second) {
            for (auto it = range.first; it != range.second; ++it) {
                if (it->second.checkParameters(argumentTypes)) {
                    std::string call = it->second.call(mUUIDCounter, argumentPayloads);
                    if (!call.empty())
                        return call;
                }
            }
        }

        // Must be a texture
        IG_ASSERT(mParent->mTree.context().Options.Scene->texture(name) != nullptr, "Expected a valid texture name");
        mUsedTextures.insert(name);
        return "color_to_vec4(" + tex_name(mParent->mTree.getClosureID(name)) + "(" + (argumentPayloads.empty() ? std::string("ctx") : ("ctx.{uvw=vec2_to_3(" + argumentPayloads[0] + ", 0)}")) + "))";
    }

    /// a.xyz Access operator for vector types
    std::string onAccess(const std::string& v, size_t inputSize, const std::vector<uint8>& outputPermutation) override
    {
        // Check if it is an identity permutation?
        if (inputSize == outputPermutation.size()) {
            bool isIdent = true;
            for (uint8 i = 0; i < (uint8)outputPermutation.size() && isIdent; ++i) {
                if (outputPermutation[i] != i)
                    isIdent = false;
            }

            if (isIdent)
                return v;
        }

        const auto getC = [=](size_t i) -> std::string {
            switch (inputSize) {
            default:
                return "a";
            case 2:
                return "vec2_at(a," + std::to_string(i) + ")";
            case 3:
                return "vec3_at(a," + std::to_string(i) + ")";
            case 4:
                return "vec4_at(a," + std::to_string(i) + ")";
            }
        };

        std::string preamble = "(@|| { let a = " + v + "; ";

        if (outputPermutation.size() == 1) {
            return preamble + getC(outputPermutation[0]) + "})()";
        } else if (outputPermutation.size() == 2) {
            return preamble + "make_vec2("
                   + getC(outputPermutation[0]) + ", "
                   + getC(outputPermutation[1]) + ")})()";
        } else if (outputPermutation.size() == 3) {
            return preamble + "make_vec3("
                   + getC(outputPermutation[0]) + ", "
                   + getC(outputPermutation[1]) + ", "
                   + getC(outputPermutation[2]) + ")})()";
        } else {
            return preamble + "make_vec4("
                   + getC(outputPermutation[0]) + ", "
                   + getC(outputPermutation[1]) + ", "
                   + getC(outputPermutation[2]) + ", "
                   + getC(outputPermutation[3]) + ")})()";
        }
    }
};

class TranspilerInternal {
public:
    PExpr::Environment Environment;
    Transpiler* Parent;

    TranspilerInternal(Transpiler* transpiler)
        : Parent(transpiler)
    {
        Environment.registerVariableLookupFunction(std::bind(&TranspilerInternal::variableLookup, this, std::placeholders::_1));
        Environment.registerFunctionLookupFunction(std::bind(&TranspilerInternal::functionLookup, this, std::placeholders::_1));
    }

    std::optional<PExpr::VariableDef> variableLookup(const PExpr::VariableLookup& lkp)
    {
        // First check for internal variables
        if (sInternalVariables.count(lkp.name()))
            return PExpr::VariableDef(lkp.name(), sInternalVariables.at(lkp.name()).Type);

        // Check for custom variables
        if (Parent->mCustomVariableBool.count(lkp.name()))
            return PExpr::VariableDef(lkp.name(), PExprType::Boolean);
        if (Parent->mCustomVariableNumber.count(lkp.name()))
            return PExpr::VariableDef(lkp.name(), PExprType::Number);
        if (Parent->mCustomVariableVector.count(lkp.name()))
            return PExpr::VariableDef(lkp.name(), PExprType::Vec3);
        if (Parent->mCustomVariableColor.count(lkp.name()))
            return PExpr::VariableDef(lkp.name(), PExprType::Vec4);

        // Check for textures/nodes in the variable table
        if (Parent->mTree.context().Options.Scene->texture(lkp.name()))
            return PExpr::VariableDef(lkp.name(), PExprType::Vec4);

        return {};
    }

    std::optional<PExpr::FunctionDef> functionLookup(const PExpr::FunctionLookup& lkp)
    {
        const auto range = sInternalFunctions.equal_range(lkp.name());
        if (range.first != range.second) {
            // First check exact=true
            for (auto it = range.first; it != range.second; ++it) {
                auto var = it->second.matchDef(lkp, true);
                if (var.has_value())
                    return var;
            }

            // Now check exact=false
            for (auto it = range.first; it != range.second; ++it) {
                auto var = it->second.matchDef(lkp, false);
                if (var.has_value())
                    return var;
            }
        }

        // Add all texture/nodes to the function table as well, such that the uv can be changed directly
        if (Parent->mTree.context().Options.Scene->texture(lkp.name()) && lkp.matchParameter({ PExprType::Vec2 }))
            return PExpr::FunctionDef(lkp.name(), PExprType::Vec4, { PExprType::Vec2 });

        return {};
    }
};

Transpiler::Transpiler(ShadingTree& tree)
    : mTree(tree)
    , mInternal(std::make_unique<TranspilerInternal>(this))
{
}

Transpiler::~Transpiler()
{
}

std::optional<Transpiler::Result> Transpiler::transpile(const std::string& expr) const
{
    // Parse
    auto ast = mInternal->Environment.parse(expr);
    if (ast == nullptr)
        return {};

    // Transpile
    ArticVisitor visitor(this);
    std::string res = mInternal->Environment.transpile(ast, &visitor);

    // Patch output
    bool scalar_output = false;
    switch (ast->returnType()) {
    case PExprType::Number:
        scalar_output = true;
        break;
    case PExprType::Integer:
        scalar_output = true;
        res           = res + " as f32";
        break;
    case PExprType::Vec3:
        res = "vec3_to_color(" + res + ")";
        break;
    case PExprType::Vec4:
        res = "vec4_to_color(" + res + ")";
        break;
    default:
        IG_LOG(L_ERROR) << "Expression does not return a number or color, but '" << PExpr::toString(ast->returnType()) << "' instead" << std::endl;
        res = "color_builtins::pink";
        break;
    }

    return Result{ res, std::move(visitor.usedTextures()), visitor.usedVariables(), scalar_output };
}

bool Transpiler::checkIfColor(const std::string& expr) const
{
    auto ast = mInternal->Environment.parse(expr);
    if (!ast)
        return true;

    switch (ast->returnType()) {
    default:
    case PExprType::Vec3:
    case PExprType::Vec4:
        return true;
    case PExprType::Number:
    case PExprType::Integer:
        return false;
    }
}

std::string Transpiler::availableVariables()
{
    std::stringstream stream;
    for (const auto& p : sInternalVariables)
        stream << p.first << ":" << dumpType(p.second.Type) << std::endl;
    return stream.str();
}

std::string Transpiler::availableFunctions()
{
    std::stringstream stream;
    for (const auto& p : sInternalFunctions)
        stream << p.second.signature() << std::endl;
    return stream.str();
}

std::string Transpiler::generateTestShader()
{
    std::stringstream stream;

    // Dump variables
    stream << "#[export]" << std::endl
           << "fn _var_test_() -> () { " << std::endl
           << "  let ctx = make_miss_shading_context(make_empty_pixelcoord(), make_zero_ray());" << std::endl;

    for (const auto& p : sInternalVariables)
        stream << "  let _var_" << p.first << " = " << p.second.Map << ";" << std::endl;
    stream << "}" << std::endl
           << std::endl;

    // Dump out functions
    for (const auto& p : sInternalFunctions) {
        const auto& def = p.second;
        stream << "#[export]" << std::endl
               << "fn _test_" << p.first << "(";

        if (p.first == "check_ray_flag") {
            // Special case
        } else if (p.first == "transform_point" || p.first == "transform_direction" || p.first == "transform_normal") {
            // Special case
            stream << "v0: " << typeArtic(def.Arguments[0]);
        } else if (p.first == "lookup") {
            // Special case
            for (size_t i = 1; i < def.Arguments.size(); ++i) {
                stream << "v" << i - 1 << ": " << typeArtic(def.Arguments[i]);
                if (i < def.Arguments.size() - 1)
                    stream << ", ";
            }
        } else {
            for (size_t i = 0; i < def.Arguments.size(); ++i) {
                stream << "v" << i << ": " << typeArtic(def.Arguments[i]);
                if (i < def.Arguments.size() - 1)
                    stream << ", ";
            }
        }

        stream << ") -> " << typeArtic(def.ReturnType) << " {" << std::endl
               << "  let ctx = make_miss_shading_context(make_empty_pixelcoord(), make_zero_ray());" << std::endl
               << "  maybe_unused(ctx);" << std::endl;

        ParamArr arguments;
        arguments.resize(def.Arguments.size());
        if (p.first == "check_ray_flag") {
            arguments[0] = "'camera'";
        } else if (p.first == "transform_point" || p.first == "transform_direction" || p.first == "transform_normal") {
            arguments[0] = "v0";
            arguments[1] = "'global'";
            arguments[2] = "'object'";
        } else if (p.first == "lookup") {
            arguments[0] = "'linear'";
            for (size_t i = 1; i < def.Arguments.size(); ++i)
                arguments[i] = "v" + std::to_string(i - 1);
        } else {
            for (size_t i = 0; i < def.Arguments.size(); ++i)
                arguments[i] = "v" + std::to_string(i);
        }

        size_t counter = 0;
        stream << "  " << def.call(counter, arguments) << std::endl;

        stream << "}" << std::endl
               << std::endl;
    }

    return stream.str();
}
} // namespace IG