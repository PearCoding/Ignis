#pragma once

#include "IG_Config.h"
#include <unordered_map>
#include <unordered_set>

namespace IG {
class ShadingTree;
class IG_LIB Transpiler {
    friend class TranspilerInternal;
    friend class ArticVisitor;

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

    /// Returns true if the expression is evaluated as color (vec4)
    bool checkIfColor(const std::string& expr) const;

    /// Return reference to associated shading tree
    inline const ShadingTree& tree() const { return mTree; }

    inline void registerCustomVariableBool(const std::string& name, const std::string& value) { mCustomVariableBool[name] = value; }
    inline void registerCustomVariableNumber(const std::string& name, const std::string& value) { mCustomVariableNumber[name] = value; }
    inline void registerCustomVariableVector(const std::string& name, const std::string& value) { mCustomVariableVector[name] = value; }
    inline void registerCustomVariableColor(const std::string& name, const std::string& value) { mCustomVariableColor[name] = value; }

    /// Return list of available variables and their respective types
    static std::string availableVariables();
    /// Return list of available functions and their respective signatures
    static std::string availableFunctions();

    /// Return shader to check correctness of all signatures and used functions. Only useful for internal purposes
    static std::string generateTestShader();

private:
    ShadingTree& mTree;
    std::unordered_map<std::string, std::string> mCustomVariableBool;
    std::unordered_map<std::string, std::string> mCustomVariableNumber;
    std::unordered_map<std::string, std::string> mCustomVariableVector;
    std::unordered_map<std::string, std::string> mCustomVariableColor;

    std::unique_ptr<class TranspilerInternal> mInternal;
};
} // namespace IG