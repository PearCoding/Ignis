#pragma once

#include "IG_Config.h"
#include <unordered_set>

namespace IG {
class ShadingTree;
class IG_LIB Transpiler {
    friend class TranspilerInternal;

public:
    explicit Transpiler(ShadingTree& tree);
    ~Transpiler();

    struct Result {
        std::string Expr;
        std::unordered_set<std::string> Textures;  // Textures used by the expression
        std::unordered_set<std::string> Variables; // Variables used by the expression. Constants will be omitted
        bool ScalarOutput;                         // Else it is a color
    };

    /// Transpile the given expression to artic code.
    std::optional<Result> transpile(const std::string& expr) const;

    bool checkIfColor(const std::string& expr) const;

    inline const ShadingTree& tree() const { return mTree; }

    inline void registerCustomVariableBool(const std::string& name) { mCustomVariableBool.insert(name); }
    inline void registerCustomVariableNumber(const std::string& name) { mCustomVariableNumber.insert(name); }
    inline void registerCustomVariableVector(const std::string& name) { mCustomVariableVector.insert(name); }
    inline void registerCustomVariableColor(const std::string& name) { mCustomVariableColor.insert(name); }

    /// Return list of available variables and their respective types
    static std::string availableVariables();
    /// Return list of available functions and their respective signatures
    static std::string availableFunctions();

    /// Return shader to check correctness of all signatures and used functions. Only useful for internal purposes
    static std::string generateTestShader();

private:
    ShadingTree& mTree;
    std::unordered_set<std::string> mCustomVariableBool;
    std::unordered_set<std::string> mCustomVariableNumber;
    std::unordered_set<std::string> mCustomVariableVector;
    std::unordered_set<std::string> mCustomVariableColor;

    std::unique_ptr<class TranspilerInternal> mInternal;
};
} // namespace IG