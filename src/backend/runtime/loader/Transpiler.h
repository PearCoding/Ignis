#pragma once

#include "IG_Config.h"
#include <unordered_set>

namespace IG {
struct LoaderContext;
class Transpiler {
    friend class TranspilerInternal;
public:
    explicit Transpiler(const LoaderContext& ctx);
    ~Transpiler();

    struct Result {
        std::string Expr;
        std::unordered_set<std::string> Textures;
        bool ScalarOutput; // Else it is a color
    };

    /// Transpile the given expression to artic code. 
    std::optional<Result> transpile(const std::string& expr, const std::string& uv_access, bool hasSurfaceInfo) const;

    inline const LoaderContext& context() const { return mContext; }

    inline void registerCustomVariableBool(const std::string& name) { mCustomVariableBool.insert(name); }
    inline void registerCustomVariableNumber(const std::string& name) { mCustomVariableNumber.insert(name); }
    inline void registerCustomVariableVector(const std::string& name) { mCustomVariableVector.insert(name); }
    inline void registerCustomVariableColor(const std::string& name) { mCustomVariableColor.insert(name); }

private:
    const LoaderContext& mContext;
    std::unordered_set<std::string> mCustomVariableBool;
    std::unordered_set<std::string> mCustomVariableNumber;
    std::unordered_set<std::string> mCustomVariableVector;
    std::unordered_set<std::string> mCustomVariableColor;

    std::unique_ptr<class TranspilerInternal> mInternal;
};
} // namespace IG