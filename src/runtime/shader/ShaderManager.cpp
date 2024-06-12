#include "ShaderManager.h"
#include "Logger.h"
#include "StringUtils.h"

#include <fstream>

namespace IG {
ShaderManager::ShaderManager(ScriptCompiler* compiler, size_t threads, DumpLevel dumpLevel)
    : mInternalCompiler(compiler)
    , mThreads(threads)
    , mDumpLevel(dumpLevel)
{
}

static inline void dumpShader(const std::string& filename, const std::string& shader)
{
    try {
        std::ofstream stream(filename);
        stream << shader;
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        // Ignore else
    }
}

void ShaderManager::add(const std::string& id, const std::string& script, const std::string& function)
{
    if (mDumpLevel == DumpLevel::Light)
        dumpShader(whitespace_escaped(id) + ".art", script);

    const std::string full_script = mInternalCompiler->prepare(script);

    if (mDumpLevel == DumpLevel::Full)
        dumpShader(whitespace_escaped(id) + ".art", full_script);

    if (mThreads == 1) {
        // Fallback internal compilation
        IG_LOG(L_DEBUG) << "Compiling '" << id << "'" << std::endl;
        void* ptr      = mInternalCompiler->compile(full_script, function);
        mResultMap[id] = Result{
            .Log = {}, // TODO
            .Ptr = ptr,
        };
    } else {
        std::lock_guard<std::mutex> _guard(mWorkMutex);
        mWorkQueue.emplace_back(Work{
            .ID       = id,
            .Script   = full_script,
            .Function = function,
            .Check    = false });

        // Start threads
    }
}

bool ShaderManager::waitForFinish()
{
    if (mThreads != 1) {
    }

    // Return true if all compilations were successful
    for (const auto& p : mResultMap) {
        if (p.second.Ptr == nullptr)
            return false;
    }
    return true;
}

void* ShaderManager::getResult(const std::string& id) const
{
    if (const auto it = mResultMap.find(id); it != mResultMap.end())
        return it->second.Ptr;
    return nullptr;
}

std::string ShaderManager::getLog(const std::string& id) const
{
    if (const auto it = mResultMap.find(id); it != mResultMap.end())
        return it->second.Log;
    return {};
}
} // namespace IG