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
    { "uv", { "SPECIAL", PExpr::ElementaryType::Vec2, false } },
    { "pi", { "flt_pi", PExpr::ElementaryType::Number, false } },
    { "flt_eps", { "flt_eps", PExpr::ElementaryType::Number, false } },
    { "flt_max", { "flt_max", PExpr::ElementaryType::Number, false } },
    { "flt_min", { "flt_min", PExpr::ElementaryType::Number, false } },
    { "flt_inf", { "flt_inf", PExpr::ElementaryType::Number, false } }
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
using InternalDynFunction1 = InternalDynFunction<MapFunction1>;
inline static InternalDynFunction1 genDynMapFunction1(const char* func, bool withInt)
{
    return {
        withInt ? genMapFunction1(func, PExpr::ElementaryType::Integer) : nullptr,
        genMapFunction1(func, PExpr::ElementaryType::Number),
        genMapFunction1(func, PExpr::ElementaryType::Vec2),
        genMapFunction1(func, PExpr::ElementaryType::Vec3),
        genMapFunction1(func, PExpr::ElementaryType::Vec4)
    };
};
static std::unordered_map<std::string, InternalDynFunction1> sInternalDynFunctions1 = {
    { "sin", genDynMapFunction1("math_builtins::sin", false) },
    { "cos", genDynMapFunction1("math_builtins::cos", false) },
    { "tan", genDynMapFunction1("math_builtins::tan", false) },
    { "asin", genDynMapFunction1("math_builtins::asin", false) },
    { "acos", genDynMapFunction1("math_builtins::acos", false) },
    { "atan", genDynMapFunction1("math_builtins::atan", false) },
    { "pow", genDynMapFunction1("math_builtins::pow", false) },
    { "exp", genDynMapFunction1("math_builtins::exp", false) },
    { "exp2", genDynMapFunction1("math_builtins::exp2", false) },
    { "log", genDynMapFunction1("math_builtins::log", false) },
    { "log2", genDynMapFunction1("math_builtins::log2", false) },
    { "log10", genDynMapFunction1("math_builtins::log10", false) },
    { "floor", genDynMapFunction1("math_builtins::floor", false) },
    { "ceil", genDynMapFunction1("math_builtins::ceil", false) },
    { "round", genDynMapFunction1("math_builtins::round", false) },
    { "sqrt", genDynMapFunction1("math_builtins::sqrt", false) },
    { "cbrt", genDynMapFunction1("math_builtins::cbrt", false) },
    { "abs", genDynMapFunction1("math_builtins::fabs", false) }
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
using InternalDynFunction2 = InternalDynFunction<MapFunction2>;
inline static InternalDynFunction2 genDynMapFunction2(const char* func, bool withInt)
{
    return {
        withInt ? genMapFunction2(func, PExpr::ElementaryType::Integer) : nullptr,
        genMapFunction2(func, PExpr::ElementaryType::Number),
        genMapFunction2(func, PExpr::ElementaryType::Vec2),
        genMapFunction2(func, PExpr::ElementaryType::Vec3),
        genMapFunction2(func, PExpr::ElementaryType::Vec4)
    };
};
static std::unordered_map<std::string, InternalDynFunction2> sInternalDynFunctions2 = {
    { "min", genDynMapFunction2("math_builtins::fmax", false) },
    { "max", genDynMapFunction2("math_builtins::fmin", false) },
    { "atan2", genDynMapFunction2("math_builtins::atan2", false) }
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
    std::unordered_set<std::string> mUsedTextures;

public:
    inline explicit ArticVisitor(const LoaderContext& ctx, const std::string& uv_access)
        : mContext(ctx)
        , mUVAccess(uv_access)
    {
    }

    inline std::unordered_set<std::string>& usedTextures() { return mUsedTextures; }

    std::string onVariable(const std::string& name, PExpr::ElementaryType) override
    {
        auto var = sInternalVariables.find(name);
        if (var != sInternalVariables.end()) {
            if (var->first == "uv")
                return mUVAccess;
            else
                return var->second.access();
        }

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
        // Constructors
        if (name == "vec2" && argumentPayloads.size() == 2)
            return "make_vec2(" + argumentPayloads[0] + "," + argumentPayloads[1] + ")";
        if (name == "vec3" && argumentPayloads.size() == 3)
            return "make_vec3(" + argumentPayloads[0] + "," + argumentPayloads[1] + "," + argumentPayloads[2] + ")";
        if (name == "vec4" && argumentPayloads.size() == 4)
            return "make_vec4(" + argumentPayloads[0] + "," + argumentPayloads[1] + "," + argumentPayloads[2] + "," + argumentPayloads[3] + ")";
        if (name == "color" && argumentPayloads.size() == 3)
            return "make_vec4(" + argumentPayloads[0] + "," + argumentPayloads[1] + "," + argumentPayloads[2] + ", 1)";
        if (name == "color" && argumentPayloads.size() == 4)
            return "make_vec4(" + argumentPayloads[0] + "," + argumentPayloads[1] + "," + argumentPayloads[2] + "," + argumentPayloads[3] + ")";

        // Dyn Functions
        if (argumentPayloads.size() == 1) {
            auto df1 = sInternalDynFunctions1.find(name);
            if (df1 != sInternalDynFunctions1.end()) {
                switch (argumentTypes[0]) {
                case PExpr::ElementaryType::Integer:
                    if (df1->second.MapInt)
                        return df1->second.MapInt(argumentPayloads[0]);
                    break;
                case PExpr::ElementaryType::Number:
                    if (df1->second.MapNum)
                        return df1->second.MapNum(argumentPayloads[0]);
                    break;
                case PExpr::ElementaryType::Vec2:
                    if (df1->second.MapVec2)
                        return df1->second.MapVec2(argumentPayloads[0]);
                    break;
                case PExpr::ElementaryType::Vec3:
                    if (df1->second.MapVec3)
                        return df1->second.MapVec3(argumentPayloads[0]);
                    break;
                case PExpr::ElementaryType::Vec4:
                    if (df1->second.MapVec4)
                        return df1->second.MapVec4(argumentPayloads[0]);
                    break;
                default:
                    break;
                }
            }
        } else if (argumentPayloads.size() == 2) {
            auto df2 = sInternalDynFunctions2.find(name);
            if (df2 != sInternalDynFunctions2.end()) {
                switch (argumentTypes[0]) {
                case PExpr::ElementaryType::Integer:
                    if (df2->second.MapInt)
                        return df2->second.MapInt(argumentPayloads[0], argumentPayloads[1]);
                    break;
                case PExpr::ElementaryType::Number:
                    if (df2->second.MapNum)
                        return df2->second.MapNum(argumentPayloads[0], argumentPayloads[1]);
                    break;
                case PExpr::ElementaryType::Vec2:
                    if (df2->second.MapVec2)
                        return df2->second.MapVec2(argumentPayloads[0], argumentPayloads[1]);
                    break;
                case PExpr::ElementaryType::Vec3:
                    if (df2->second.MapVec3)
                        return df2->second.MapVec3(argumentPayloads[0], argumentPayloads[1]);
                    break;
                case PExpr::ElementaryType::Vec4:
                    if (df2->second.MapVec4)
                        return df2->second.MapVec4(argumentPayloads[0], argumentPayloads[1]);
                    break;
                default:
                    break;
                }
            }
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

std::optional<Transpiler::Result> Transpiler::transpile(const std::string& expr, const std::string& uv_access) const
{
    PExpr::Environment env;

    // Add internal variables to the variable table
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
    env.registerDef(PExpr::FunctionDef("vec2", PExpr::ElementaryType::Vec2, { PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));
    env.registerDef(PExpr::FunctionDef("vec3", PExpr::ElementaryType::Vec3, { PExpr::ElementaryType::Number, PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));
    env.registerDef(PExpr::FunctionDef("vec4", PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Number, PExpr::ElementaryType::Number, PExpr::ElementaryType::Number, PExpr::ElementaryType::Number }));
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

    // Add all texture/nodes to the function table as well, such that the uv can be changed directly
    for (const auto& tex : mContext.Scene.textures())
        env.registerDef(PExpr::FunctionDef(tex.first, PExpr::ElementaryType::Vec4, { PExpr::ElementaryType::Vec2 }));

    // Parse
    auto ast = env.parse(expr);
    if (ast == nullptr)
        return {};

    // Transpile
    ArticVisitor visitor(mContext, uv_access);
    std::string res = env.transpile(ast, &visitor);

    // Prepare output
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