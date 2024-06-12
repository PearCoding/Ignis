#pragma once

#include "ScriptCompiler.h"

#include <thread>

namespace IG {
class ShaderManager {
public:
    enum DumpLevel {
        None,
        Light,
        Full
    };

    ShaderManager(ScriptCompiler* compiler, size_t threads, DumpLevel dumpLevel = DumpLevel::None);
    ~ShaderManager();

    void add(const std::string& id, const std::string& script, const std::string& function);

    bool waitForFinish();

    void* getResult(const std::string& id) const;
    std::string getLog(const std::string& id) const;

private:
    std::unique_ptr<class ShaderManagerInternal> mInternal;
    const size_t mThreadCount;
    const ShaderManager::DumpLevel mDumpLevel;
};
} // namespace IG