#include "ScriptPreprocessor.h"
#include "Logger.h"
#include <fstream>

// Will be populated by api_collector
extern const char* ig_api[];
extern const char* ig_api_paths[];

namespace IG {
std::string ScriptPreprocessor::prepare(const std::string& script)
{
    std::stringstream source;

    if (mStdLibOverride.empty()) {
        for (int i = 0; ig_api[i]; ++i)
            source << ig_api[i];
    } else {
        source << mStdLibOverride;
    }

    source << std::endl;
    source << script;

    return source.str();
}

static inline bool checkShaderFileName(const std::filesystem::path& path)
{
    if (path.empty())
        return false;

    if (path.extension() != ".art")
        return false;

    if (path.filename().generic_string()[0] == '.')
        return false;

    if (path.stem().generic_string() == "dummy_main")
        return false;

    return true;
}

void ScriptPreprocessor::loadStdLibFromDirectory(const std::filesystem::path& dir)
{
    std::stringstream lib;

    // Fixme: This should not be handled here
    // Add only the first file from the (internal api) as it contains runtime specific stuff (which should be in the AnyDSL runtime library)
    lib << ig_api[0];

    // Iterate through given directory in search for *.art files. Files with a starting . are ignored
    for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied)) {
        if (dirEntry.is_regular_file()) {
            const auto& path = dirEntry.path();
            if (checkShaderFileName(path) && dirEntry.exists()) {
                IG_LOG(L_DEBUG) << "Adding " << path << " to standard library" << std::endl;

                lib << std::endl;
                lib << std::ifstream(path).rdbuf();
            }
        }
    }

    mStdLibOverride = lib.str();
}

} // namespace IG