#include "ShaderBuilder.h"
#include "Logger.h"

namespace IG {
ShaderBuilder::ShaderBuilder()
{
}

ShaderBuilder::~ShaderBuilder()
{
}

ShaderBuilder& ShaderBuilder::merge(const ShaderBuilder& other)
{
    mIncludes.insert(other.mIncludes.begin(), other.mIncludes.end());
    mStatements.insert(mStatements.end(), other.mStatements.begin(), other.mStatements.end());
    mFunctions.insert(other.mFunctions.begin(), other.mFunctions.end()); // TODO: Check for collisions?

    return *this;
}

ShaderBuilder& ShaderBuilder::addFunction(const std::string& def, const ShaderBuilder& body)
{
    if (mFunctions.count(def) > 0) {
        IG_LOG(L_ERROR) << "ShaderBuilder: Given function def '" << def << "' already exists" << std::endl;
        return *this;
    }

    ShaderBuilder copy = body;

    mIncludes.insert(copy.mIncludes.begin(), copy.mIncludes.end());
    copy.mIncludes.clear();
    mFunctions[def] = std::make_shared<ShaderBuilder>(std::move(copy));

    return *this;
}

ShaderBuilder& ShaderBuilder::addStatement(const std::string& statement)
{
    mStatements.emplace_back(statement);
    return *this;
}

ShaderBuilder& ShaderBuilder::addStatements(const std::vector<std::string>& statements)
{
    mStatements.insert(mStatements.end(), statements.begin(), statements.end());
    return *this;
}

ShaderBuilder& ShaderBuilder::addInclude(const std::string& inc)
{
    mIncludes.insert(inc);
    return *this;
}

void ShaderBuilder::build(std::ostream& stream) const
{
    for (const auto& inc : mIncludes)
        stream << "//#<include=\"" << inc << "\"" << std::endl;

    for (const auto& func : mFunctions) {
        stream << "fn @" << func.first << "{" << std::endl;
        func.second->build(stream);
        stream << "}" << std::endl;
    }

    for (const auto& statement : mStatements)
        stream << statement << std::endl;
}
} // namespace IG