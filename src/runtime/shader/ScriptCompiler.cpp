#include "ScriptCompiler.h"
#include "Logger.h"
#include "RuntimeInfo.h"
#include "device/ICompilerDevice.h"
#include <fstream>

// Will be populated by api_collector
extern const char* ig_api[];
extern const char* ig_api_paths[];

namespace IG {
ScriptCompiler::ScriptCompiler(const std::shared_ptr<ICompilerDevice>& compiler)
    : mCompiler(compiler)
    , mStdLibOverride()
    , mOptimizationLevel(3)
    , mVerbose(false)
{
}

ScriptCompiler::~ScriptCompiler()
{
}

void* ScriptCompiler::compile(const std::string& script, const std::string& function) const
{
    // AnyDSL has no support for multi-threaded compile process :/
    std::lock_guard<std::mutex> _guard(mCompileMutex);

    return mCompiler->compile(
        ICompilerDevice::Settings{
            .OptimizationLevel = (int)mOptimizationLevel,
            .Verbose           = mVerbose },
        script, function);
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

static inline bool checkShaderFileName(const Path& path)
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

void ScriptCompiler::loadStdLibFromDirectory(const Path& dir)
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