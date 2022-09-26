#include "ScriptCompiler.h"
#include "Logger.h"
#include <fstream>

#include <anydsl_jit.h>
#include <anydsl_runtime.hpp>

// Will be populated by api_collector
extern const char* ig_api[];
extern const char* ig_api_paths[];

namespace IG {
ScriptCompiler::ScriptCompiler() 
    : mStdLibOverride()
    , mOptimizationLevel(3)
    , mVerbose(false)
{

}

ScriptCompiler::~ScriptCompiler() {

}

void* ScriptCompiler::compile(const std::string& script, const std::string& function) const
{
#ifdef IG_DEBUG
    anydsl_set_log_level(mVerbose ? 1 /* info */ : 4 /* error */);
#else
    anydsl_set_log_level(mVerbose ? 3 /* warn */ : 4 /* error */);
#endif

    const int ret = anydsl_compile(script.c_str(), (uint32_t)script.length(), (uint32_t)mOptimizationLevel);
    if (ret < 0)
        return nullptr;

    void* callback = anydsl_lookup_function(ret, function.c_str());
    if (callback == nullptr)
        IG_LOG(L_ERROR) << "Could not find function '" << function << "' in compiled script" << std::endl;

    return callback;
}

std::string ScriptCompiler::prepare(const std::string& script) const
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

void ScriptCompiler::loadStdLibFromDirectory(const std::filesystem::path& dir)
{
    std::stringstream lib;

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