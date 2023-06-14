#include "ScriptCompiler.h"
#include "Logger.h"
#include "RuntimeInfo.h"
#include <fstream>

#include "device/AnyDSLRuntime.h"

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

ScriptCompiler::~ScriptCompiler()
{
}

void* ScriptCompiler::compile(const std::string& script, const std::string& function) const
{
    // AnyDSL has no support for multi-threaded compile process :/
    std::lock_guard<std::mutex> _guard(mCompileMutex);

    static bool once = false;
    if (!once) {
        const auto module_path = RuntimeInfo::modulePath();
        if (!module_path.empty()) {
            IG_LOG(L_DEBUG) << "Loading symbolic module " << module_path << std::endl;
            AnyDSLJITLinkInfo info{
                AnyDSL_STRUCTURE_TYPE_JIT_LINK_INFO,
                nullptr,
                module_path.generic_string().c_str()
            };
            IG_CHECK_ANYDSL(anydslLinkJITLibrary(AnyDSL_NULL_HANDLE, 1, &info));
        }
        once = true;
    }

    const auto cache_dir = RuntimeInfo::cacheDirectory();

    AnyDSLJITCompileOptions options = {
        AnyDSL_STRUCTURE_TYPE_JIT_COMPILE_OPTIONS,
        nullptr,
        (uint32_t)mOptimizationLevel,
#if 0//def IG_DEBUG
        mVerbose ? 1u /* info */ : 4u /* error */,
#else
        mVerbose ? 3u /* warn */ : 4u /* error */,
#endif
        AnyDSL_COMPILE_LANGUAGE_ARTIC_BIT,
        AnyDSL_TRUE,
        cache_dir.c_str()
    };

    AnyDSLJITCompileResult result = {
        AnyDSL_STRUCTURE_TYPE_JIT_COMPILE_RESULT,
        nullptr,
        nullptr
    };

    AnyDSLJITModule module;
    bool res = IG_CHECK_ANYDSL(anydslCompileJIT(script.c_str(), script.length(), &module, &options, &result));

    if (result.logOutput)
        IG_LOG(L_ERROR) << "JIT: " << result.logOutput << std::endl;

    IG_CHECK_ANYDSL(anydslFreeJITCompileResult(&result));

    if (!res)
        return nullptr;

    AnyDSLJITLookupInfo lookupInfo{
        AnyDSL_STRUCTURE_TYPE_JIT_LOOKUP_INFO,
        nullptr,
        nullptr
    };

    res = IG_CHECK_ANYDSL(anydslLookupJIT(module, function.c_str(), &lookupInfo));

    if (!res || lookupInfo.pHandle == nullptr)
        IG_LOG(L_ERROR) << "Could not find function '" << function << "' in compiled script" << std::endl;

    return const_cast<void*>(lookupInfo.pHandle);
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