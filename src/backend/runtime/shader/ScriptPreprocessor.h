#pragma once

#include "IG_Config.h"

namespace IG {
class ScriptPreprocessor {
public:
    ScriptPreprocessor()  = default;
    ~ScriptPreprocessor() = default;

    std::string prepare(const std::string& script) const;
    void loadStdLibFromDirectory(const std::filesystem::path& dir);

private:
    std::string mStdLibOverride;
};
} // namespace IG