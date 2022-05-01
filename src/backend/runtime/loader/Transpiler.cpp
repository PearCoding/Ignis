#include "Transpiler.h"
#include "LoaderContext.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include "PExpr.h"

namespace IG {
inline std::string tex_name(const std::string& name)
{
    return "tex_" + ShaderUtils::escapeIdentifier(name);
}

struct InternalVariable {
    std::string Name;
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
static InternalVariable sInternalVariables[] = {
    { "uv", "SPECIAL", PExpr::ElementaryType::Vec2, false },
    { "", "", PExpr::ElementaryType::Unspecified, false }
};
static const InternalVariable* getInternalVariable(const std::string& name)
{
    for (size_t i = 0; sInternalVariables[i].Type != PExpr::ElementaryType::Unspecified; ++i) {
        if (sInternalVariables[i].Name == name)
            return &sInternalVariables[i];
    }
    return nullptr;
}

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
        auto var = getInternalVariable(name);
        if (var != nullptr) {
            if (var->Name == "uv")
                return mUVAccess;
            else
                return var->access();
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
                               PExpr::ElementaryType, const std::vector<PExpr::ElementaryType>&,
                               const std::vector<std::string>& argumentPayloads) override
    {
        // TODO: Add internal functions

        // Must be a texture
        IG_ASSERT(argumentPayloads.size() == 1, "Expected a valid texture access");
        mUsedTextures.insert(name);
        return "color_to_vec4(" + tex_name(name) + "(" + argumentPayloads[0] + "))"; // Error should be caught in type checking
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
    for (size_t i = 0; sInternalVariables[i].Type != PExpr::ElementaryType::Unspecified; ++i) {
        const auto& var = sInternalVariables[i];
        env.registerDef(PExpr::VariableDef(var.Name, var.Type));
    }

    // Add all textures/nodes to the variable table
    for (const auto& tex : mContext.Scene.textures()) {
        if (getInternalVariable(tex.first) != nullptr)
            IG_LOG(L_WARNING) << "Ignoring texture '" << tex.first << "' as it conflicts with internal variable name" << std::endl;
        else
            env.registerDef(PExpr::VariableDef(tex.first, PExpr::ElementaryType::Vec4));
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