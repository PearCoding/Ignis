#include "Transpiler.h"
#include "LoaderContext.h"
#include "LoaderUtils.h"
#include "Logger.h"

#include "PExpr.h"

#include <unordered_map>

namespace IG {
using PExprType = PExpr::ElementaryType;

inline std::string tex_name(const std::string& name)
{
    return "tex_" + LoaderUtils::escapeIdentifier(name);
}
inline std::string var_name(const std::string& name)
{
    return "var_tex_" + LoaderUtils::escapeIdentifier(name);
}

// Internal Variables
struct InternalVariable {
    std::string Map;
    PExprType Type;
    bool IsColor;

    inline std::string access() const
    {
        if (IsColor && Type == PExprType::Vec4)
            return "color_to_vec4(" + Map + ")";
        else
            return Map;
    }
};
static const std::unordered_map<std::string, InternalVariable> sInternalVariables = {
    { "uv", { "", PExprType::Vec2, false } }, // Special
    { "P", { "", PExprType::Vec3, false } },  // Special
    { "N", { "", PExprType::Vec3, false } },  // Special
    { "Pi", { "flt_pi", PExprType::Number, false } },
    { "E", { "flt_e", PExprType::Number, false } },
    { "Eps", { "flt_eps", PExprType::Number, false } },
    { "NumMax", { "flt_max", PExprType::Number, false } },
    { "NumMin", { "flt_min", PExprType::Number, false } },
    { "Inf", { "flt_inf", PExprType::Number, false } }
};

// Dyn Functions
template <typename MapF>
struct InternalDynFunction {
    MapF MapInt;
    MapF MapNum;
    MapF MapVec2;
    MapF MapVec3;
    MapF MapVec4;
};

template <typename MapF, typename... Args>
inline static std::string callDynFunction(const InternalDynFunction<MapF>& df, PExprType type, const Args&... args)
{
    switch (type) {
    case PExprType::Integer:
        if (df.MapInt)
            return df.MapInt(args...);
        break;
    case PExprType::Number:
        if (df.MapNum)
            return df.MapNum(args...);
        break;
    case PExprType::Vec2:
        if (df.MapVec2)
            return df.MapVec2(args...);
        break;
    case PExprType::Vec3:
        if (df.MapVec3)
            return df.MapVec3(args...);
        break;
    case PExprType::Vec4:
        if (df.MapVec4)
            return df.MapVec4(args...);
        break;
    default:
        break;
    }
    return {};
}
std::optional<PExpr::FunctionDef> matchFuncRet(const PExpr::FunctionLookup& lkp, PExprType retType, const std::vector<PExprType>& params)
{
    if (lkp.matchParameter(params))
        return PExpr::FunctionDef(lkp.name(), retType, params);
    return {};
}

using OpParam = std::optional<PExprType>;
template <typename MapF, typename... Args>
inline std::optional<PExpr::FunctionDef> checkDynFunction(const PExpr::FunctionLookup& lkp, const InternalDynFunction<MapF>& df,
                                                          const OpParam& retType, bool fixI, const std::optional<Args>&... params)
{
    if (df.MapInt && lkp.matchParameter({ params.value_or(PExprType::Integer)... }, true))
        return PExpr::FunctionDef(lkp.name(), fixI ? PExprType::Integer : retType.value_or(PExprType::Integer), { params.value_or(PExprType::Integer)... });
    if (df.MapNum && lkp.matchParameter({ params.value_or(PExprType::Number)... }, false))
        return PExpr::FunctionDef(lkp.name(), retType.value_or(PExprType::Number), { params.value_or(PExprType::Number)... });
    if (df.MapVec2 && lkp.matchParameter({ params.value_or(PExprType::Vec2)... }, true))
        return PExpr::FunctionDef(lkp.name(), retType.value_or(PExprType::Vec2), { params.value_or(PExprType::Vec2)... });
    if (df.MapVec3 && lkp.matchParameter({ params.value_or(PExprType::Vec3)... }, true))
        return PExpr::FunctionDef(lkp.name(), retType.value_or(PExprType::Vec3), { params.value_or(PExprType::Vec3)... });
    if (df.MapVec4 && lkp.matchParameter({ params.value_or(PExprType::Vec4)... }, true))
        return PExpr::FunctionDef(lkp.name(), retType.value_or(PExprType::Vec4), { params.value_or(PExprType::Vec4)... });

    return {};
}

// Dyn Functions 1
using MapFunction1 = std::function<std::string(const std::string&)>;
inline static MapFunction1 genMapFunction1(const char* func, PExprType type)
{
    switch (type) {
    default:
        return {};
    case PExprType::Integer:
    case PExprType::Number:
        return [=](const std::string& a) { return std::string(func) + "(" + a + ")"; };
    case PExprType::Vec2:
        return [=](const std::string& a) { return "vec2_map(" + a + ", |x:f32| " + std::string(func) + "(x))"; };
    case PExprType::Vec3:
        return [=](const std::string& a) { return "vec3_map(" + a + ", |x:f32| " + std::string(func) + "(x))"; };
    case PExprType::Vec4:
        return [=](const std::string& a) { return "vec4_map(" + a + ", |x:f32| " + std::string(func) + "(x))"; };
    }
}
inline static MapFunction1 genMapFunction1(const char* func, const char* funcI, PExprType type)
{
    if (type == PExprType::Integer)
        return [=](const std::string& a) { return std::string(funcI) + "(" + a + ")"; };
    else
        return genMapFunction1(func, type);
}
inline static MapFunction1 genFunction1(const char* func)
{
    return [=](const std::string& a) { return std::string(func) + "(" + a + ")"; };
}
inline static MapFunction1 genFunction1Color(const char* func)
{
    return [=](const std::string& a) { return "color_to_vec4(" + std::string(func) + "(" + a + "))"; };
}
inline static MapFunction1 genFunction1ColorIO(const char* func)
{
    return [=](const std::string& a) { return "color_to_vec4(" + std::string(func) + "(vec4_to_color(" + a + ")))"; };
}
inline static MapFunction1 genArrayFunction1(const char* func, PExprType type)
{
    switch (type) {
    default:
        return {};
    case PExprType::Vec2:
        return [=](const std::string& a) { return "vec2_" + std::string(func) + "(" + a + ")"; };
    case PExprType::Vec3:
        return [=](const std::string& a) { return "vec3_" + std::string(func) + "(" + a + ")"; };
    case PExprType::Vec4:
        return [=](const std::string& a) { return "vec4_" + std::string(func) + "(" + a + ")"; };
    }
}
using InternalDynFunction1 = InternalDynFunction<MapFunction1>;
inline static InternalDynFunction1 genDynMapFunction1(const char* func, const char* funcI)
{
    return {
        funcI ? genMapFunction1(func, funcI, PExprType::Integer) : nullptr,
        genMapFunction1(func, PExprType::Number),
        genMapFunction1(func, PExprType::Vec2),
        genMapFunction1(func, PExprType::Vec3),
        genMapFunction1(func, PExprType::Vec4)
    };
}
inline static InternalDynFunction1 genDynArrayFunction1(const char* func)
{
    return {
        nullptr,
        nullptr,
        genArrayFunction1(func, PExprType::Vec2),
        genArrayFunction1(func, PExprType::Vec3),
        genArrayFunction1(func, PExprType::Vec4)
    };
}

// Dyn Functions 2
using MapFunction2         = std::function<std::string(const std::string&, const std::string&)>;
using InternalDynFunction2 = InternalDynFunction<MapFunction2>;
inline static MapFunction2 genMapFunction2(const char* func, PExprType type)
{
    switch (type) {
    default:
        return {};
    case PExprType::Integer:
    case PExprType::Number:
        return [=](const std::string& a, const std::string& b) { return std::string(func) + "(" + a + ", " + b + ")"; };
    case PExprType::Vec2:
        return [=](const std::string& a, const std::string& b) { return "vec2_zip(" + a + ", " + b + ", |x:f32, y:f32| " + std::string(func) + "(x, y))"; };
    case PExprType::Vec3:
        return [=](const std::string& a, const std::string& b) { return "vec3_zip(" + a + ", " + b + ", |x:f32, y:f32| " + std::string(func) + "(x, y))"; };
    case PExprType::Vec4:
        return [=](const std::string& a, const std::string& b) { return "vec4_zip(" + a + ", " + b + ", |x:f32, y:f32| " + std::string(func) + "(x, y))"; };
    }
}
inline static MapFunction2 genMapFunction2(const char* func, const char* funcI, PExprType type)
{
    if (type == PExprType::Integer)
        return [=](const std::string& a, const std::string& b) { return std::string(funcI) + "(" + a + ", " + b + ")"; };
    else
        return genMapFunction2(func, type);
}
inline static MapFunction2 genArrayFunction2(const char* func, PExprType type)
{
    switch (type) {
    default:
        return {};
    case PExprType::Vec2:
        return [=](const std::string& a, const std::string& b) { return "vec2_" + std::string(func) + "(" + a + ", " + b + ")"; };
    case PExprType::Vec3:
        return [=](const std::string& a, const std::string& b) { return "vec3_" + std::string(func) + "(" + a + ", " + b + ")"; };
    case PExprType::Vec4:
        return [=](const std::string& a, const std::string& b) { return "vec4_" + std::string(func) + "(" + a + ", " + b + ")"; };
    }
}
inline static MapFunction2 genFunction2(const char* func)
{
    return [=](const std::string& a, const std::string& b) { return std::string(func) + "(" + a + ", " + b + ")"; };
}
inline static MapFunction2 genFunction2Color(const char* func)
{
    return [=](const std::string& a, const std::string& b) { return "color_to_vec4(" + std::string(func) + "(" + a + ", " + b + "))"; };
}
inline static InternalDynFunction2 genDynMapFunction2(const char* func, const char* funcI)
{
    return {
        funcI ? genMapFunction2(func, funcI, PExprType::Integer) : nullptr,
        genMapFunction2(func, PExprType::Number),
        genMapFunction2(func, PExprType::Vec2),
        genMapFunction2(func, PExprType::Vec3),
        genMapFunction2(func, PExprType::Vec4)
    };
}
inline static InternalDynFunction2 genDynArrayFunction2(const char* func)
{
    return {
        nullptr,
        nullptr,
        genArrayFunction2(func, PExprType::Vec2),
        genArrayFunction2(func, PExprType::Vec3),
        genArrayFunction2(func, PExprType::Vec4)
    };
}

// Dyn Functions 3
using MapFunction3         = std::function<std::string(const std::string&, const std::string&, const std::string&)>;
using InternalDynFunction3 = InternalDynFunction<MapFunction3>;
inline static MapFunction3 genMapFunction3(const char* func, PExprType type)
{
    switch (type) {
    default:
        return {};
    case PExprType::Integer:
    case PExprType::Number:
        return [=](const std::string& a, const std::string& b, const std::string& c) { return std::string(func) + "(" + a + ", " + b + ", " + c + ")"; };
    case PExprType::Vec2:
        return [=](const std::string& a, const std::string& b, const std::string& c) { return "vec2_" + std::string(func) + "(" + a + ", " + b + ", " + c + ")"; };
    case PExprType::Vec3:
        return [=](const std::string& a, const std::string& b, const std::string& c) { return "vec3_" + std::string(func) + "(" + a + ", " + b + ", " + c + ")"; };
    case PExprType::Vec4:
        return [=](const std::string& a, const std::string& b, const std::string& c) { return "vec4_" + std::string(func) + "(" + a + ", " + b + ", " + c + ")"; };
    }
}
// Type A func (A, A, num)
inline static InternalDynFunction3 genDynLerpFunction3(const char* func)
{
    return {
        nullptr,
        genMapFunction3(func, PExprType::Number),
        genMapFunction3(func, PExprType::Vec2),
        genMapFunction3(func, PExprType::Vec3),
        genMapFunction3(func, PExprType::Vec4)
    };
}

// Defs
static const std::unordered_map<std::string, InternalDynFunction1> sInternalDynFunctions1 = {
    { "sin", genDynMapFunction1("math_builtins::sin", nullptr) },
    { "cos", genDynMapFunction1("math_builtins::cos", nullptr) },
    { "tan", genDynMapFunction1("math_builtins::tan", nullptr) },
    { "asin", genDynMapFunction1("math_builtins::asin", nullptr) },
    { "acos", genDynMapFunction1("math_builtins::acos", nullptr) },
    { "atan", genDynMapFunction1("math_builtins::atan", nullptr) },
    { "pow", genDynMapFunction1("math_builtins::pow", nullptr) },
    { "exp", genDynMapFunction1("math_builtins::exp", nullptr) },
    { "exp2", genDynMapFunction1("math_builtins::exp2", nullptr) },
    { "log", genDynMapFunction1("math_builtins::log", nullptr) },
    { "log2", genDynMapFunction1("math_builtins::log2", nullptr) },
    { "log10", genDynMapFunction1("math_builtins::log10", nullptr) },
    { "floor", genDynMapFunction1("math_builtins::floor", nullptr) },
    { "ceil", genDynMapFunction1("math_builtins::ceil", nullptr) },
    { "round", genDynMapFunction1("math_builtins::round", nullptr) },
    { "sqrt", genDynMapFunction1("math_builtins::sqrt", nullptr) },
    { "cbrt", genDynMapFunction1("math_builtins::cbrt", nullptr) },
    { "abs", genDynMapFunction1("math_builtins::fabs", "abs") },
    { "rad", genDynMapFunction1("rad", nullptr) },
    { "deg", genDynMapFunction1("deg", nullptr) },
    { "norm", genDynArrayFunction1("normalize") },
    { "hash", { nullptr, genMapFunction1("hash_rndf", PExprType::Number), nullptr, nullptr, nullptr } },
    { "smoothstep", { nullptr, genMapFunction1("smoothstep", PExprType::Number), nullptr, nullptr, nullptr } },
    { "smootherstep", { nullptr, genMapFunction1("smootherstep", PExprType::Number), nullptr, nullptr, nullptr } },
    { "rgbtoxyz", { nullptr, nullptr, nullptr, nullptr, genFunction1ColorIO("srgb_to_xyz") } },
    { "xyztorgb", { nullptr, nullptr, nullptr, nullptr, genFunction1ColorIO("xyz_to_srgb") } },
    { "rgbtohsv", { nullptr, nullptr, nullptr, nullptr, genFunction1ColorIO("srgb_to_hsv") } },
    { "hsvtorgb", { nullptr, nullptr, nullptr, nullptr, genFunction1ColorIO("hsv_to_srgb") } },
    { "rgbtohsl", { nullptr, nullptr, nullptr, nullptr, genFunction1ColorIO("srgb_to_hsl") } },
    { "hsltorgb", { nullptr, nullptr, nullptr, nullptr, genFunction1ColorIO("hsl_to_srgb") } }
};

static const std::unordered_map<std::string, InternalDynFunction2> sInternalDynFunctions2 = {
    { "min", genDynMapFunction2("math_builtins::fmax", "min") },
    { "max", genDynMapFunction2("math_builtins::fmin", "max") },
    { "atan2", genDynMapFunction2("math_builtins::atan2", nullptr) },
    { "cross", { nullptr, nullptr, nullptr, genArrayFunction2("cross", PExprType::Vec3), nullptr } }
};
static const std::unordered_map<std::string, InternalDynFunction2> sInternalDynNoiseFunctions2 = {
    { "noise", { nullptr, genFunction2("noise1"), genFunction2("noise2_v"), genFunction2("noise3_v"), nullptr } },
    { "snoise", { nullptr, genFunction2("snoise1"), genFunction2("snoise2"), genFunction2("snoise3"), nullptr } },
    { "pnoise", { nullptr, genFunction2("pnoise1"), genFunction2("pnoise2"), genFunction2("pnoise3"), nullptr } },
    { "cellnoise", { nullptr, genFunction2("cellnoise1"), genFunction2("cellnoise2"), genFunction2("cellnoise3"), nullptr } },
    { "perlin", { nullptr, nullptr, genFunction2("perlin2"), nullptr /*TODO*/, nullptr } },
    { "sperlin", { nullptr, nullptr, genFunction2("sperlin2"), nullptr /*TODO*/, nullptr } },
    { "voronoi", { nullptr, nullptr, genFunction2("voronoi2"), nullptr, nullptr } },
    { "fbm", { nullptr, nullptr, genFunction2("fbm2"), nullptr, nullptr } }
};
static const std::unordered_map<std::string, InternalDynFunction1> sInternalDynColoredNoiseFunctions1 = {
    { "cnoise", { nullptr, genFunction1Color("cnoise1_def"), genFunction1Color("cnoise2_def"), genFunction1Color("cnoise3_def"), nullptr } },
    { "cpnoise", { nullptr, genFunction1Color("cpnoise1_def"), genFunction1Color("cpnoise2_def"), genFunction1Color("cpnoise3_def"), nullptr } },
    { "ccellnoise", { nullptr, genFunction1Color("ccellnoise1_def"), genFunction1Color("ccellnoise2_def"), genFunction1Color("ccellnoise3_def"), nullptr } },
    { "cperlin", { nullptr, nullptr, genFunction1Color("cperlin2_def"), nullptr /*TODO*/, nullptr } },
    { "cvoronoi", { nullptr, nullptr, genFunction1Color("cvoronoi2_def"), nullptr, nullptr } },
    { "cfbm", { nullptr, nullptr, genFunction1Color("cfbm2_def"), nullptr, nullptr } }
};
static const std::unordered_map<std::string, InternalDynFunction2> sInternalDynColoredNoiseFunctions2 = {
    { "cnoise", { nullptr, genFunction2Color("cnoise1"), genFunction2Color("cnoise2"), genFunction2Color("cnoise3"), nullptr } },
    { "cpnoise", { nullptr, genFunction2Color("cpnoise1"), genFunction2Color("cpnoise2"), genFunction2Color("cpnoise3"), nullptr } },
    { "ccellnoise", { nullptr, genFunction2Color("ccellnoise1"), genFunction2Color("ccellnoise2"), genFunction2Color("ccellnoise3"), nullptr } },
    { "cvoronoi", { nullptr, nullptr, genFunction2Color("cvoronoi2"), nullptr, nullptr } },
    { "cfbm", { nullptr, nullptr, genFunction2Color("cfbm2"), nullptr, nullptr } }
};

static const std::unordered_map<std::string, InternalDynFunction3> sInternalDynFunctions3 = {
    { "clamp", { genMapFunction3("clamp", PExprType::Integer), genMapFunction3("clampf", PExprType::Number), nullptr, nullptr, nullptr } }
};
static const std::unordered_map<std::string, InternalDynFunction3> sInternalDynLerpFunctions3 = {
    { "mix", genDynLerpFunction3("lerp") }
};

static const std::unordered_map<std::string, InternalDynFunction1> sInternalDynReduceFunctions1 = {
    { "length", genDynArrayFunction1("len") },
    { "sum", genDynArrayFunction1("sum") },
    { "avg", genDynArrayFunction1("avg") },
    { "checkerboard", { nullptr, nullptr, genFunction1("node_checkerboard"), nullptr, nullptr } },
    { "noise", { nullptr, genFunction1("noise1_def"), genFunction1("noise2_def"), genFunction1("noise3_def"), nullptr } },
    { "snoise", { nullptr, genFunction1("snoise1_def"), genFunction1("snoise2_def"), genFunction1("snoise3_def"), nullptr } },
    { "pnoise", { nullptr, genFunction1("pnoise1_def"), genFunction1("pnoise2_def"), genFunction1("pnoise3_def"), nullptr } },
    { "cellnoise", { nullptr, genFunction1("cellnoise1_def"), genFunction1("cellnoise2_def"), genFunction1("cellnoise3_def"), nullptr } },
    { "perlin", { nullptr, nullptr, genFunction1("perlin2_def"), nullptr /* TODO: 3D Version */, nullptr } },
    { "sperlin", { nullptr, nullptr, genFunction1("sperlin2_def"), nullptr /* TODO: 3D Version */, nullptr } },
    { "voronoi", { nullptr, nullptr, genFunction1("voronoi2_def"), nullptr, nullptr } },
    { "fbm", { nullptr, nullptr, genFunction1("fbm2_def"), nullptr, nullptr } }
};
static const std::unordered_map<std::string, InternalDynFunction2> sInternalDynReduceFunctions2 = {
    { "angle", genDynArrayFunction2("angle") },
    { "dot", genDynArrayFunction2("dot") },
    { "dist", genDynArrayFunction2("dist") }
};

// Other stuff
inline std::string binaryCwise(const std::string& A, const std::string& B, PExprType arithType, const std::string& op, const std::string& func)
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
    const LoaderContext& mContext;
    const std::string mUVAccess;
    const bool mHasSurfaceInfo;
    std::unordered_set<std::string> mUsedTextures;

public:
    inline explicit ArticVisitor(const LoaderContext& ctx, const std::string& uv_access, bool hasSurfaceInfo)
        : mContext(ctx)
        , mUVAccess(uv_access)
        , mHasSurfaceInfo(hasSurfaceInfo)
    {
    }

    inline std::unordered_set<std::string>& usedTextures() { return mUsedTextures; }

    std::string onVariable(const std::string& name, PExprType expectedType) override
    {
        if (name == "uv")
            return mUVAccess;

        if (mHasSurfaceInfo) {
            if (name == "P")
                return "surf.point";
            if (name == "N")
                return "surf.local.col(2)";
        }

        auto var = sInternalVariables.find(name);
        if (var != sInternalVariables.end())
            return var->second.access();

        if (expectedType == PExprType::Vec4 && mContext.Scene.texture(name) != nullptr) {
            mUsedTextures.insert(name);
            return "color_to_vec4(" + tex_name(name) + "(" + mUVAccess + "))";
        } else {
            if (expectedType == PExprType::Vec4)
                return "color_to_vec4(" + var_name(name) + ")";
            else
                return var_name(name);
        }
    }

    std::string onInteger(PExpr::Integer v) override { return std::to_string(v) + ":i32"; }
    std::string onNumber(PExpr::Number v) override { return std::to_string(v) + ":f32"; }
    std::string onBool(bool v) override { return v ? "true" : "false"; }
    std::string onString(const std::string& v) override { return "\"" + v + "\""; }

    /// Implicit casts. Currently only int -> num
    std::string onCast(const std::string& v, PExprType, PExprType) override
    {
        return "((" + v + ") as f32)";
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
        if (isSub)
            return binaryCwise(a, b, arithType, "-", "sub");
        else
            return binaryCwise(a, b, arithType, "+", "add");
    }

    /// a*b, a/b. Only called for arithmetic types. Both types are the same! Vectorized types should apply component wise
    std::string onMulDiv(bool isDiv, PExprType arithType, const std::string& a, const std::string& b) override
    {
        if (isDiv)
            return binaryCwise(a, b, arithType, "/", "div");
        else
            return binaryCwise(a, b, arithType, "*", "mul");
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
            return "(math_builtins::pow(" + a + " as f32, " + f + " as f32) as i32)";
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
        if (argumentPayloads.size() == 1) {
            auto df1 = sInternalDynFunctions1.find(name);
            if (df1 != sInternalDynFunctions1.end()) {
                std::string call = callDynFunction(df1->second, argumentTypes[0], argumentPayloads[0]);
                if (!call.empty())
                    return call;
            }

            auto drf1 = sInternalDynReduceFunctions1.find(name);
            if (drf1 != sInternalDynReduceFunctions1.end()) {
                std::string call = callDynFunction(drf1->second, argumentTypes[0], argumentPayloads[0]);
                if (!call.empty())
                    return call;
            }

            auto dcnf1 = sInternalDynColoredNoiseFunctions1.find(name);
            if (dcnf1 != sInternalDynColoredNoiseFunctions1.end()) {
                std::string call = callDynFunction(dcnf1->second, argumentTypes[0], argumentPayloads[0]);
                if (!call.empty())
                    return call;
            }

            if (name == "vec2")
                return "make_vec2(" + argumentPayloads[0] + "," + argumentPayloads[0] + ")";
            if (name == "vec3")
                return "make_vec3(" + argumentPayloads[0] + "," + argumentPayloads[0] + "," + argumentPayloads[0] + ")";
            if (name == "vec4")
                return "make_vec4(" + argumentPayloads[0] + "," + argumentPayloads[0] + "," + argumentPayloads[0] + "," + argumentPayloads[0] + ")";
            if (name == "color")
                return "make_vec4(" + argumentPayloads[0] + "," + argumentPayloads[0] + "," + argumentPayloads[0] + ", 1)";
        } else if (argumentPayloads.size() == 2) {
            auto df2 = sInternalDynFunctions2.find(name);
            if (df2 != sInternalDynFunctions2.end()) {
                std::string call = callDynFunction(df2->second, argumentTypes[0], argumentPayloads[0], argumentPayloads[1]);
                if (!call.empty())
                    return call;
            }

            auto drf2 = sInternalDynReduceFunctions2.find(name);
            if (drf2 != sInternalDynReduceFunctions2.end()) {
                std::string call = callDynFunction(drf2->second, argumentTypes[0], argumentPayloads[0], argumentPayloads[1]);
                if (!call.empty())
                    return call;
            }

            auto dnf2 = sInternalDynNoiseFunctions2.find(name);
            if (dnf2 != sInternalDynNoiseFunctions2.end()) {
                std::string call = callDynFunction(dnf2->second, argumentTypes[0], argumentPayloads[0], argumentPayloads[1]);
                if (!call.empty())
                    return call;
            }

            auto dcnf2 = sInternalDynColoredNoiseFunctions2.find(name);
            if (dcnf2 != sInternalDynColoredNoiseFunctions2.end()) {
                std::string call = callDynFunction(dcnf2->second, argumentTypes[0], argumentPayloads[0], argumentPayloads[1]);
                if (!call.empty())
                    return call;
            }

            if (name == "vec2")
                return "make_vec2(" + argumentPayloads[0] + "," + argumentPayloads[1] + ")";
        } else if (argumentPayloads.size() == 3) {
            auto df3 = sInternalDynFunctions3.find(name);
            if (df3 != sInternalDynFunctions3.end()) {
                std::string call = callDynFunction(df3->second, argumentTypes[0], argumentPayloads[0], argumentPayloads[1], argumentPayloads[2]);
                if (!call.empty())
                    return call;
            }

            auto dlf3 = sInternalDynLerpFunctions3.find(name);
            if (dlf3 != sInternalDynLerpFunctions3.end()) {
                std::string call = callDynFunction(dlf3->second, argumentTypes[0], argumentPayloads[0], argumentPayloads[1], argumentPayloads[2]);
                if (!call.empty())
                    return call;
            }

            if (name == "vec3")
                return "make_vec3(" + argumentPayloads[0] + "," + argumentPayloads[1] + "," + argumentPayloads[2] + ")";
            if (name == "color")
                return "make_vec4(" + argumentPayloads[0] + "," + argumentPayloads[1] + "," + argumentPayloads[2] + ", 1)";
            if (name == "select")
                return "select(" + argumentPayloads[0] + "," + argumentPayloads[1] + "," + argumentPayloads[2] + ")";
        } else if (argumentPayloads.size() == 4) {
            if (name == "vec4")
                return "make_vec4(" + argumentPayloads[0] + "," + argumentPayloads[1] + "," + argumentPayloads[2] + "," + argumentPayloads[3] + ")";
            if (name == "color")
                return "make_vec4(" + argumentPayloads[0] + "," + argumentPayloads[1] + "," + argumentPayloads[2] + "," + argumentPayloads[3] + ")";
        }

        // Must be a texture
        IG_ASSERT(mContext.Scene.texture(name) != nullptr, "Expected a valid texture name");
        mUsedTextures.insert(name);
        return "color_to_vec4(" + tex_name(name) + "(" + (argumentPayloads.empty() ? mUVAccess : argumentPayloads[0]) + "))";
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

        // Check for textures/nodes to the variable table
        if (Parent->mContext.Scene.texture(lkp.name()))
            return PExpr::VariableDef(lkp.name(), PExprType::Vec4);

        return {};
    }

    std::optional<PExpr::FunctionDef> functionLookup(const PExpr::FunctionLookup& lkp)
    {
        // Add some internal functions
        if (lkp.name() == "vec2") {
            auto var = matchFuncRet(lkp, PExprType::Vec2, { PExprType::Number });
            if (var.has_value())
                return var;

            var = matchFuncRet(lkp, PExprType::Vec2, { PExprType::Number, PExprType::Number });
            if (var.has_value())
                return var;
        }

        if (lkp.name() == "vec3") {
            auto var = matchFuncRet(lkp, PExprType::Vec3, { PExprType::Number });
            if (var.has_value())
                return var;

            var = matchFuncRet(lkp, PExprType::Vec3, { PExprType::Number, PExprType::Number, PExprType::Number });
            if (var.has_value())
                return var;
        }

        if (lkp.name() == "vec4" || lkp.name() == "color") {
            auto var = matchFuncRet(lkp, PExprType::Vec4, { PExprType::Number });
            if (var.has_value())
                return var;

            var = matchFuncRet(lkp, PExprType::Vec4, { PExprType::Number, PExprType::Number, PExprType::Number, PExprType::Number });
            if (var.has_value())
                return var;

            if (lkp.name() == "color") {
                var = matchFuncRet(lkp, PExprType::Vec4, { PExprType::Number, PExprType::Number, PExprType::Number });
                if (var.has_value())
                    return var;
            }
        }

        if (sInternalDynFunctions1.count(lkp.name())) {
            auto var = checkDynFunction(lkp, sInternalDynFunctions1.at(lkp.name()), OpParam{}, false,
                                        OpParam{});
            if (var.has_value())
                return var;
        }

        if (sInternalDynFunctions2.count(lkp.name())) {
            auto var = checkDynFunction(lkp, sInternalDynFunctions2.at(lkp.name()), OpParam{}, false,
                                        OpParam{}, OpParam{});
            if (var.has_value())
                return var;
        }

        if (sInternalDynFunctions3.count(lkp.name())) {
            auto var = checkDynFunction(lkp, sInternalDynFunctions3.at(lkp.name()), OpParam{}, false,
                                        OpParam{}, OpParam{}, OpParam{});
            if (var.has_value())
                return var;
        }

        // Reduce functions
        if (sInternalDynReduceFunctions1.count(lkp.name())) {
            auto var = checkDynFunction(lkp, sInternalDynReduceFunctions1.at(lkp.name()), PExprType::Number, true,
                                        OpParam{});
            if (var.has_value())
                return var;
        }

        if (sInternalDynReduceFunctions2.count(lkp.name())) {
            auto var = checkDynFunction(lkp, sInternalDynReduceFunctions2.at(lkp.name()), PExprType::Number, true,
                                        OpParam{}, OpParam{});
            if (var.has_value())
                return var;
        }

        // Always vec4 foo(A), with A can be num, vec2 or vec3
        if (sInternalDynColoredNoiseFunctions1.count(lkp.name())) {
            auto var = checkDynFunction(lkp, sInternalDynColoredNoiseFunctions1.at(lkp.name()), PExprType::Vec4, true,
                                        OpParam{});
            if (var.has_value())
                return var;
        }

        // Always num foo(A, num), with A can be num, vec2 or vec3
        if (sInternalDynColoredNoiseFunctions1.count(lkp.name())) {
            auto var = checkDynFunction(lkp, sInternalDynColoredNoiseFunctions1.at(lkp.name()), PExprType::Number, true,
                                        OpParam{}, OpParam{ PExprType::Number });
            if (var.has_value())
                return var;
        }

        // Always vec4 foo(A, num), with A can be num, vec2 or vec3
        if (sInternalDynColoredNoiseFunctions2.count(lkp.name())) {
            auto var = checkDynFunction(lkp, sInternalDynColoredNoiseFunctions2.at(lkp.name()), PExprType::Vec4, true,
                                        OpParam{}, OpParam{ PExprType::Number });
            if (var.has_value())
                return var;
        }

        if (sInternalDynLerpFunctions3.count(lkp.name())) {
            auto var = checkDynFunction(lkp, sInternalDynLerpFunctions3.at(lkp.name()), OpParam{}, true,
                                        OpParam{}, OpParam{}, OpParam{ PExprType::Number });
            if (var.has_value())
                return var;
        }

        // Select function
        if (lkp.name() == "select" && lkp.parameters().size() == 3) {
            auto var = matchFuncRet(lkp, PExprType::Boolean, { PExprType::Boolean, PExprType::Boolean, PExprType::Boolean });
            if (var.has_value())
                return var;

            var = matchFuncRet(lkp, PExprType::String, { PExprType::Boolean, PExprType::String, PExprType::String });
            if (var.has_value())
                return var;

            var = matchFuncRet(lkp, PExprType::Integer, { PExprType::Boolean, PExprType::Integer, PExprType::Integer });
            if (var.has_value())
                return var;

            var = matchFuncRet(lkp, PExprType::Number, { PExprType::Boolean, PExprType::Number, PExprType::Number });
            if (var.has_value())
                return var;

            var = matchFuncRet(lkp, PExprType::Vec2, { PExprType::Boolean, PExprType::Vec2, PExprType::Vec2 });
            if (var.has_value())
                return var;

            var = matchFuncRet(lkp, PExprType::Vec3, { PExprType::Boolean, PExprType::Vec3, PExprType::Vec3 });
            if (var.has_value())
                return var;

            var = matchFuncRet(lkp, PExprType::Vec4, { PExprType::Boolean, PExprType::Vec4, PExprType::Vec4 });
            if (var.has_value())
                return var;
        }

        // Add all texture/nodes to the function table as well, such that the uv can be changed directly
        if (Parent->mContext.Scene.texture(lkp.name()) && lkp.matchParameter({ PExprType::Vec2 }))
            return PExpr::FunctionDef(lkp.name(), PExprType::Vec4, { PExprType::Vec2 });

        return {};
    }
};

Transpiler::Transpiler(const LoaderContext& ctx)
    : mContext(ctx)
    , mInternal(std::make_unique<TranspilerInternal>(this))
{
}

Transpiler::~Transpiler()
{
}

std::optional<Transpiler::Result> Transpiler::transpile(const std::string& expr, const std::string& uv_access, bool hasSurfaceInfo) const
{
    // Parse
    auto ast = mInternal->Environment.parse(expr);
    if (ast == nullptr)
        return {};

    // Transpile
    ArticVisitor visitor(mContext, uv_access, hasSurfaceInfo);
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
    case PExprType::Vec4:
        res = "vec4_to_color(" + res + ")";
        break;
    default:
        IG_LOG(L_ERROR) << "Expression does not return a number or color, but '" << PExpr::toString(ast->returnType()) << "' instead" << std::endl;
        res = "color_builtins::pink";
        break;
    }

    return Result{ res, std::move(visitor.usedTextures()), scalar_output };
}
} // namespace IG