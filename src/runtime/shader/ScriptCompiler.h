#pragma once

#include "IG_Config.h"

#include <mutex>

namespace IG {
class ICompilerDevice;

class IG_LIB ScriptCompiler {
public:
    explicit ScriptCompiler(const std::shared_ptr<ICompilerDevice>& compiler, const Path& stdlibDir= {});
    ~ScriptCompiler();

    inline void setOptimizationLevel(size_t l) { mOptimizationLevel = l; }
    inline size_t optimizationLevel() const { return mOptimizationLevel; }

    inline void setVerbose(bool verbose) { mVerbose = verbose; }
    inline bool isVerbose() const { return mVerbose; }

    inline void setStdLibPath(const Path& p) { mStdLibPath = p; }
    inline const Path& stdLibPath() const { return mStdLibPath; }

    std::string prepare(const std::string& script, const std::string& path = {}) const;
    void* compile(const std::string& script, const std::string& function) const;

private:
    std::string prepare(const std::string_view& script, const std::string_view& path, std::unordered_set<std::string>& included_paths) const;

    std::shared_ptr<ICompilerDevice> mCompiler;

    Path mStdLibPath;
    size_t mOptimizationLevel;
    bool mVerbose;

    mutable std::mutex mCompileMutex;
};
} // namespace IG