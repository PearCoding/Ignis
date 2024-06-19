#pragma once

#include "IG_Config.h"

namespace IG {
class IG_LIB ShaderBuilder {
public:
    ShaderBuilder();
    ~ShaderBuilder();

    ShaderBuilder& merge(const ShaderBuilder& other);

    ShaderBuilder& addFunction(const std::string& def, const ShaderBuilder& body);
    ShaderBuilder& addStatement(const std::string& statement);
    ShaderBuilder& addStatements(const std::vector<std::string>& statements);
    ShaderBuilder& addInclude(const std::string& inc);

    void build(std::ostream& stream) const;

    inline bool isRoot() const { return mStatements.empty(); }

private:
    std::unordered_map<std::string, std::shared_ptr<ShaderBuilder>> mFunctions;
    std::vector<std::string> mStatements;
    std::unordered_set<std::string> mIncludes;
};
} // namespace IG