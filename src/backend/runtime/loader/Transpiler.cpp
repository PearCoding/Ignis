#include "Transpiler.h"
#include "LoaderContext.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include "PExpr.h"

#include <unordered_map>

namespace IG {
inline std::string tex_name(const std::string& name)
{
    return "tex_" + ShaderUtils::escapeIdentifier(name);
}

// Internal Variables
struct InternalVariable {
    std::string Map;
    PExpr::ElementaryType Type;
    bool IsColor;

    inline std::string access() const
    {
        if (IsColor && Type == PExpr::ElementaryType::Vec4)
            return "color_to_vec4(" + Map + ")";
        else
            return Map;
    }
};
static std::unordered_map<std::string, InternalVariable> sInternalVariables = {
    { "Pi", { "flt_pi", PExpr::ElementaryType::Number, false } },
    { "E", { "flt_e", PExpr::ElementaryType::Number, false } },
    { "Eps", { "flt_eps", PExpr::ElementaryType::Number, false } },
    { "NumMax", { "flt_max", PExpr::ElementaryType::Number, false } },
    { "NumMin", { "flt_min", PExpr::ElementaryType::Number, false } },
    { "Inf", { "flt_inf", PExpr::ElementaryType::Number, false } }
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
inline static std::string callDynFunction(const InternalDynFunction<MapF>& df, PExpr::ElementaryType type, const Args&... args)
{
    switch (type) {
    case PExpr::ElementaryType::Integer:
        if (df.MapInt)
            return df.MapInt(args...);
        break;
    case PExpr::ElementaryType::Number:
        if (df.MapNum)
            return df.MapNum(args...);
        break;
    case PExpr::ElementaryType::Vec2:
        if (df.MapVec2)
            return df.MapVec2(args...);
        break;
    case PExpr::ElementaryType::Vec3:
        if (df.MapVec3)
            return df.MapVec3(args...);
        break;
    case PExpr::ElementaryType::Vec4:
        if (df.MapVec4)
            return df.MapVec4(args...);
        break;
    default:
        break;
    }
    return {};
}

// Dyn Functions 1
using MapFunction1 = std::function<std::string(const std::string&)>;
inline static MapFunction1 genMapFunction1(const char* func, PExpr::ElementaryType type)
{
    switch (type) {
    default:
        return {};
    case PExpr::ElementaryType::Integer:
    case PExpr::ElementaryType::Number:
        return [=](const std::string& a) { return std::string(func) + "(" + a + ")"; };
    case PExpr::ElementaryType::Vec2:
        return [=](const std::string& a) { return "vec2_map(" + a + ", |x:f32| " + std::string(func) + "(x))"; };
    case PExpr::ElementaryType::Vec3:
        return [=](const std::string& a) { return "vec3_map(" + a + ", |x:f32| " + std::string(func) + "(x))"; };
    case PExpr::ElementaryType::Vec4:
        return [=](const std::string& a) { return "vec4_map(" + a + ", |x:f32| " + std::string(func) + "(x))"; };
    }
};
inline static MapFunction1 genMapFunction1(const char* func, const char* funcI, PExpr::ElementaryType type)
{
    if (type == PExpr::ElementaryType::Integer)
        return [=](const std::string& a) { return std::string(funcI) + "(" + a + ")"; };
    else
        return genMapFunction1(func, type);
};
inline static MapFunction1 genArrayFunction1(const char* func, PExpr::ElementaryType type)
{
    switch (type) {
    default:
        return {};
    case PExpr::ElementaryType::Vec2:
        return [=](const std::string& a) { return "vec2_" + std::string(func) + "(" + a + ")"; };
    case PExpr::ElementaryType::Vec3:
        return [=](const std::string& a) { return "vec3_" + std::string(func) + "(" + a + ")"; };
    case PExpr::ElementaryType::Vec4:
        return [=](const std::string& a) { return "vec4_" + std::string(func) + "(" + a + ")"; };
    }
};
using InternalDynFunction1 = InternalDynFunction<MapFunction1>;
inline static InternalDynFunction1 genDynMapFunction1(const char* func, const char* funcI)
{
    return {
        funcI ? genMapFunction1(func, funcI, PExpr::ElementaryType::Integer) : nullptr,
        genMapFunction1(func, PExpr::ElementaryType::Number),
        genMapFunction1(func, PExpr::ElementaryType::Vec2),
        genMapFunction1(func, PExpr::ElementaryType::Vec3),
        genMapFunction1(func, PExpr::ElementaryType::Vec4)
    };
};
inline static InternalDynFunction1 genDynArrayFunction1(const char* func)
{
    return {
        nullptr,
        nullptr,
        genArrayFunction1(func, PExpr::ElementaryType::Vec2),
        genArrayFunction1(func, PExpr::ElementaryType::Vec3),
        genArrayFunction1(func, PExpr::ElementaryType::Vec4)
    };
};
static std::unordered_map<std::string, InternalDynFunction1> sInternalDynFunctions1 = {
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
    { "cross", { nullptr, nullptr, nullptr, genArrayFunction1("cross", PExpr::ElementaryType::Vec3), nullptr } }
};

// Dyn Functions 2
using MapFunction2 = std::function<std::string(const std::string&, const std::string&)>;
inline static MapFunction2 genMapFunction2(const char* func, PExpr::ElementaryType type)
{
    switch (type) {
    default:
        return {};
    case PExpr::ElementaryType::Integer:
    case PExpr::ElementaryType::Number:
        return [=](const std::string& a, const std::string& b) { return std::string(func) + "(" + a + ", " + b + ")"; };
    case PExpr::ElementaryType::Vec2:
        return [=](const std::string& a, const std::string& b) { return "vec2_zip(" + a + ", " + b + ", |x:f32, y:f32| " + std::string(func) + "(x, y))"; };
    case PExpr::ElementaryType::Vec3:
        return [=](const std::string& a, const std::string& b) { return "vec3_zip(" + a + ", " + b + ", |x:f32, y:f32| " + std::string(func) + "(x, y))"; };
    case PExpr::ElementaryType::Vec4:
        return [=](const std::string& a, const std::string& b) { return "vec4_zip(" + a + ", " + b + ", |x:f32, y:f32| " + std::string(func) + "(x, y))"; };
    }
};
inline static MapFunction2 genMapFunction2(const char* func, const char* funcI, PExpr::ElementaryType type)
{
    if (type == PExpr::ElementaryType::Integer)
        return [=](const std::string& a, const std::string& b) { return std::string(funcI) + "(" + a + ", " + b + ")"; };
    else
        return genMapFunction2(func, type);
};
inline static MapFunction2 genArrayFunction2(const char* func, PExpr::ElementaryType type)
{
    switch (type) {
    default:
        return {};
    case PExpr::ElementaryType::Vec2:
        return [=](const std::string& a, const std::string& b) { return "vec2_" + std::string(func) + "(" + a + ", " + b + ")"; };
    case PExpr::ElementaryType::Vec3:
        return [=](const std::string& a, const std::string& b) { return "vec3_" + std::string(func) + "(" + a + ", " + b + ")"; };
    case PExpr::ElementaryType::Vec4:
        return [=](const std::string& a, const std::string& b) { return "vec4_" + std::string(func) + "(" + a + ", " + b + ")"; };
    }
};
using InternalDynFunction2 = InternalDynFunction<MapFunction2>;
inline static InternalDynFunction2 genDynMapFunction2(const char* func, const char* funcI)
{
    return {
        funcI ? genMapFunction2(func, funcI, PExpr::ElementaryType::Integer) : nullptr,
        genMapFunction2(func, PExpr::ElementaryType::Number),
        genMapFunction2(func, PExpr::ElementaryType::Vec2),
        genMapFunction2(func, PExpr::ElementaryType::Vec3),
        genMapFunction2(func, PExpr::ElementaryType::Vec4)
    };
};
inline static InternalDynFunction2 genDynArrayFunction2(const char* func)
{
    return {
        nullptr,
        nullptr,
        genArrayFunction2(func, PExpr::ElementaryType::Vec2),
        genArrayFunction2(func, PExpr::ElementaryType::Vec3),
        genArrayFunction2(func, PExpr::ElementaryType::Vec4)
    };
};

static std::unordered_map<std::string, InternalDynFunction2> sInternalDynFunctions2 = {
    { "min", genDynMapFunction2("math_builtins::fmax", "min") },
    { "max", genDynMapFunction2("math_builtins::fmin", "max") },
    { "atan2", genDynMapFunction2("math_builtins::atan2", nullptr) }
};

// Dyn Functions 3
using MapFunction3 = std::function<std::string(const std::string&, const std::string&, const std::string&)>;
inline static MapFunction3 genMapFunction3(const char* func, PExpr::ElementaryType type)
{
    switch (type) {
    default:
        return {};
    case PExpr::ElementaryType::Integer:
    case PExpr::ElementaryType::Number:
        return [=](const std::string& a, const std::string& b, const std::string& c) { return std::string(func) + "(" + a + ", " + b + ", " + c + ")"; };
    case PExpr::ElementaryType::Vec2:
    case PExpr::ElementaryType::Vec3:
    case PExpr::ElementaryType::Vec4:
        return {}; // Currently not used
    }
};
using InternalDynFunction3                                                          = InternalDynFunction<MapFunction3>;
static std::unordered_map<std::string, InternalDynFunction3> sInternalDynFunctions3 = {
    { "clamp", { genMapFunction3("clamp", PExpr::ElementaryType::Integer), genMapFunction3("clampf", PExpr::ElementaryType::Number), nullptr, nullptr, nullptr } }
};

// Reduce functions
static std::unordered_map<std::string, InternalDynFunction1> sInternalDynReduceFunctions1 = {
    { "length", genDynArrayFunction1("len") },
    { "sum", genDynArrayFunction1("sum") },
    { "avg", genDynArrayFunction1("avg") }
};
static std::unordered_map<std::string, InternalDynFunction2> sInternalDynReduceFunctions2 = {
    { "angle", genDynArrayFunction2("angle") },
    { "dot", genDynArrayFunction2("dot") },
    { "dist", genDynArrayFunction2("dist") }
};

// Other stuff
inline std::string binaryCwise(const std::string& A, const std::string& B, PExpr::ElementaryType arithType, const std::string& op, const std::string& func)
{
    switch (arithType) {
    default:
        return "(" + A + ") " + op + " (" + B + ")";
    case PExpr::ElementaryType::Vec2:
        return "vec2_" + func + "(" + A + ", " + B + ")";
    case PExpr::ElementaryType::Vec3:
        return "vec3_" + func + "(" + A + ", " + B + ")";
    case PExpr::ElementaryType::Vec4:
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

    std::string onVariable(const std::string& name, PExpr::ElementaryType) override
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

        // Must be a texture
        mUsedTextures.insert(name);
        return "color_to_vec4(" + tex_name(name) + "(" + mUVAccess + "))";
    }

    std::string onInteger(PExpr::Integer v) override { return std::to_string(v) + ":i32"; }
    std::string onNumber(PExpr::Number v) override { return std::to_string(v) + ":f32"; }
    std::string onBool(bool v) override { return v ? "true" : "false"; }
    std::string onString(const std::string& v) override { return "\"" + v + "\""; }

    /// Implicit casts. Currently only int -> num
    std::string onCast(const std::string& v, PExpr::ElementaryType, PExpr::ElementaryType) override
    {
        return "((" + v + ") as f32)";
    }

    /// +a, -a. Only called for arithmetic types
    std::string onPosNeg(bool isNeg, PExpr::ElementaryType arithType, const std::string& v) override
    {
        if (!isNeg)
            return v;

        switch (arithType) {
        default:
            return "(-(" + v + "))";
        case PExpr::ElementaryType::Vec2:
            return "vec2_neg(" + v + ")";
        case PExpr::ElementaryType::Vec3:
            return "vec3_neg(" + v + ")";
        case PExpr::ElementaryType::Vec4:
            return "vec4_neg(" + v + ")";
        }
    }

    // !a. Only called for bool
    std::string onNot(const std::string& v) override
    {
        return "!" + v;
    }

    /// a+b, a-b. Only called for arithmetic types. Both types are the same! Vectorized types should apply component wise
    std::string onAddSub(bool isSub, PExpr::ElementaryType arithType, const std::string& a, const std::string& b) override
    {
        if (isSub)
            return binaryCwise(a, b, arithType, "-", "sub");
        else
            return binaryCwise(a, b, arithType, "+", "add");
    }

    /// a*b, a/b. Only called for arithmetic types. Both types are the same! Vectorized types should apply component wise
    std::string onMulDiv(bool isDiv, PExpr::ElementaryType arithType, const std::string& a, const std::string& b) override
    {
        if (isDiv)
            return binaryCwise(a, b, arithType, "/", "div");
        else
            return binaryCwise(a, b, arithType, "*", "mul");
    }

    /// a*f, f*a, a/f. A is an arithmetic type, f is 'num', except when a is 'int' then f is 'int' as well. Order of a*f or f*a does not matter
    std::string onScale(bool isDiv, PExpr::ElementaryType aType, const std::string& a, const std::string& f) override
    {
        if (isDiv)
            return binaryCwise(a, f, aType, "/", "divf");
        else
            return binaryCwise(a, f, aType, "*", "mulf");
    }

    /// a^f A is an arithmetic type, f is 'num', except when a is 'int' then f is 'int' as well. Vectorized types should apply component wise
    std::string onPow(PExpr::ElementaryType aType, const std::string& a, const std::string& f) override
    {
        if (aType == PExpr::ElementaryType::Integer)
            return "(math_builtins::pow(" + a + " as f32, " + f + " as f32) as i32)";
        else if (aType == PExpr::ElementaryType::Number)
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
    std::string onRelOp(PExpr::RelationalOp op, PExpr::ElementaryType, const std::string& a, const std::string& b) override
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
    std::string onEqual(bool isNeg, PExpr::ElementaryType type, const std::string& a, const std::string& b) override
    {
        std::string res = binaryCwise(a, b, type, "==", "eq");
        return isNeg ? "!" + res : res;
    }

    /// name(...). Call to a function. Necessary casts are already handled.
    std::string onFunctionCall(const std::string& name,
                               PExpr::ElementaryType, const std::vector<PExpr::ElementaryType>& argumentTypes,
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

            if (name == "vec2")
                return "make_vec2(" + argumentPayloads[0] + "," + argumentPayloads[1] + ")";
        } else if (argumentPayloads.size() == 3) {
            auto df3 = sInternalDynFunctions3.find(name);
            if (df3 != sInternalDynFunctions3.end()) {
                std::string call = callDynFunction(df3->second, argumentTypes[0], argumentPayloads[0], argumentPayloads[1], argumentPayloads[2]);
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
        IG_ASSERT(argumentPayloads.size() == 1, "Expected a valid texture access");
        mUsedTextures.insert(name);
        return "color_to_vec4(" + tex_name(name) + "(" + argumentPayloads[0] + "))";
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

std::optional<Transpiler::Result> Transpiler::transpile(const std::string& expr, const std::string& uv_access, bool hasSurfaceInfo) const
{
    PExpr::Environment env;

    // Add internal variables to the variable table
    env.registerDef(PExpr::VariableDef("uv", PExpr::ElementaryType::Vec2));
    if (hasSurfaceInfo) {
        env.registerDef(PExpr::VariableDef("P", PExpr::ElementaryType::Vec3));
        env.registerDef(PExpr::VariableDef("N", PExpr::ElementaryType::Vec3));
    }

    for (const auto& var : sInternalVariables)
        env.registerDef(PExpr::VariableDef(var.first, var.second.Type));

    // Add all textures/nodes to the variable table
    for (const auto& tex : mContext.Scene.textures()) {
        if (sInternalVariables.count(tex.first) > 0)
            IG_LOG(L_WARNING) << "Ignoring texture '" << tex.first << "' as it conflicts with internal variable name" << std::endl;
        else
            env.registerDef(PExpr::VariableDef(tex.first, PExpr::ElementaryType::Vec4));
    }

    // Add some internal functions
    // TODO: We could optimize the loading process via a definition block
    env.registerDef(PExpr::FunctionDef("vec2", PExpr::ElementaryType::Vec2, { PExpr::ElementaryType::Number }));
    env.registerDef(PExpr::FunctionDef("vec2", PExpr::ElementaryType::Vec2, { PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));
    env.registerDef(PExpr::FunctionDef("vec3", PExpr::ElementaryType::Vec3, { PExpr::ElementaryType::Number }));
    env.registerDef(PExpr::FunctionDef("vec3", PExpr::ElementaryType::Vec3, { PExpr::ElementaryType::Number, PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));
    env.registerDef(PExpr::FunctionDef("vec4", PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Number }));
    env.registerDef(PExpr::FunctionDef("vec4", PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Number, PExpr::ElementaryType::Number, PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));
    env.registerDef(PExpr::FunctionDef("color", PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Number }));
    env.registerDef(PExpr::FunctionDef("color", PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Number, PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));
    env.registerDef(PExpr::FunctionDef("color", PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Number, PExpr::ElementaryType::Number, PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));

    for (const auto& df : sInternalDynFunctions1) {
        if (df.second.MapInt)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Integer, { PExpr::ElementaryType::Integer }));
        if (df.second.MapNum)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Number, { PExpr::ElementaryType::Number }));
        if (df.second.MapVec2)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Vec2, { PExpr::ElementaryType::Vec2 }));
        if (df.second.MapVec3)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Vec3, { PExpr::ElementaryType::Vec3 }));
        if (df.second.MapVec4)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Vec4 }));
    }

    for (const auto& df : sInternalDynReduceFunctions1) {
        if (df.second.MapInt)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Integer, { PExpr::ElementaryType::Integer }));
        if (df.second.MapNum)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Number, { PExpr::ElementaryType::Number }));
        if (df.second.MapVec2)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Number, { PExpr::ElementaryType::Vec2 }));
        if (df.second.MapVec3)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Number, { PExpr::ElementaryType::Vec3 }));
        if (df.second.MapVec4)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Number, { PExpr::ElementaryType::Vec4 }));
    }

    for (const auto& df : sInternalDynFunctions2) {
        if (df.second.MapInt)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Integer, { PExpr::ElementaryType::Integer, PExpr::ElementaryType::Integer }));
        if (df.second.MapNum)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Number, { PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));
        if (df.second.MapVec2)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Vec2, { PExpr::ElementaryType::Vec2, PExpr::ElementaryType::Vec2 }));
        if (df.second.MapVec3)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Vec3, { PExpr::ElementaryType::Vec3, PExpr::ElementaryType::Vec3 }));
        if (df.second.MapVec4)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Vec4, PExpr::ElementaryType::Vec4 }));
    }

    for (const auto& df : sInternalDynReduceFunctions2) {
        if (df.second.MapInt)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Integer, { PExpr::ElementaryType::Integer, PExpr::ElementaryType::Integer }));
        if (df.second.MapNum)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Number, { PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));
        if (df.second.MapVec2)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Number, { PExpr::ElementaryType::Vec2, PExpr::ElementaryType::Vec2 }));
        if (df.second.MapVec3)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Number, { PExpr::ElementaryType::Vec3, PExpr::ElementaryType::Vec3 }));
        if (df.second.MapVec4)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Number, { PExpr::ElementaryType::Vec4, PExpr::ElementaryType::Vec4 }));
    }

    for (const auto& df : sInternalDynFunctions3) {
        if (df.second.MapInt)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Integer, { PExpr::ElementaryType::Integer, PExpr::ElementaryType::Integer, PExpr::ElementaryType::Number }));
        if (df.second.MapNum)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Number, { PExpr::ElementaryType::Number, PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));
        if (df.second.MapVec2)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Vec2, { PExpr::ElementaryType::Vec2, PExpr::ElementaryType::Vec2, PExpr::ElementaryType::Vec2 }));
        if (df.second.MapVec3)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Vec3, { PExpr::ElementaryType::Vec3, PExpr::ElementaryType::Vec3, PExpr::ElementaryType::Vec3 }));
        if (df.second.MapVec4)
            env.registerDef(PExpr::FunctionDef(df.first, PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Vec4, PExpr::ElementaryType::Vec4, PExpr::ElementaryType::Vec4 }));
    }

    // Select funciton
    env.registerDef(PExpr::FunctionDef("select", PExpr::ElementaryType::Boolean, { PExpr::ElementaryType::Boolean, PExpr::ElementaryType::Boolean, PExpr::ElementaryType::Boolean }));
    env.registerDef(PExpr::FunctionDef("select", PExpr::ElementaryType::String, { PExpr::ElementaryType::Boolean, PExpr::ElementaryType::String, PExpr::ElementaryType::String }));
    env.registerDef(PExpr::FunctionDef("select", PExpr::ElementaryType::Integer, { PExpr::ElementaryType::Boolean, PExpr::ElementaryType::Integer, PExpr::ElementaryType::Integer }));
    env.registerDef(PExpr::FunctionDef("select", PExpr::ElementaryType::Number, { PExpr::ElementaryType::Boolean, PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));
    env.registerDef(PExpr::FunctionDef("select", PExpr::ElementaryType::Vec2, { PExpr::ElementaryType::Boolean, PExpr::ElementaryType::Vec2, PExpr::ElementaryType::Vec2 }));
    env.registerDef(PExpr::FunctionDef("select", PExpr::ElementaryType::Vec3, { PExpr::ElementaryType::Boolean, PExpr::ElementaryType::Vec3, PExpr::ElementaryType::Vec3 }));
    env.registerDef(PExpr::FunctionDef("select", PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Boolean, PExpr::ElementaryType::Vec4, PExpr::ElementaryType::Vec4 }));

    // Add all texture/nodes to the function table as well, such that the uv can be changed directly
    for (const auto& tex : mContext.Scene.textures())
        env.registerDef(PExpr::FunctionDef(tex.first, PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Vec2 }));

    // Parse
    auto ast = env.parse(expr);
    if (ast == nullptr)
        return {};

    // Transpile
    ArticVisitor visitor(mContext, uv_access, hasSurfaceInfo);
    std::string res = env.transpile(ast, &visitor);

    // Patch output
    bool scalar_output = false;
    switch (ast->returnType()) {
    case PExpr::ElementaryType::Number:
        scalar_output = true;
        break;
    case PExpr::ElementaryType::Integer:
        scalar_output = true;
        res           = res + " as f32";
        break;
    case PExpr::ElementaryType::Vec4:
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