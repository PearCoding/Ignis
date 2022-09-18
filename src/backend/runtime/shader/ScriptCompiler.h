#pragma once

#include "IG_Config.h"

namespace IG {
class ScriptCompiler {
public:
    ScriptCompiler()  = default;
    ~ScriptCompiler() = default;

    std::string prepare(const std::string& script) const;
    void* compile(const std::string& script, const std::string& function, bool isVerbose) const;
    void loadStdLibFromDirectory(const std::filesystem::path& dir);

private:
    std::string mStdLibOverride;
};
} // namespace IG