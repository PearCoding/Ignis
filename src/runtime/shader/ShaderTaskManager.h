#pragma once

#include "ScriptCompiler.h"

#include <thread>

namespace IG {
class ShaderTaskManager {
public:
    enum DumpLevel {
        None,
        Light,
        Full
    };

    ShaderTaskManager(ScriptCompiler* compiler, size_t threads, DumpLevel dumpLevel = DumpLevel::None);
    ~ShaderTaskManager();

    void add(const std::string& id, const std::string& name, const std::string& script, const std::string& function);

    bool waitForFinish();

    void* getResult(const std::string& id) const;
    std::string getLog(const std::string& id) const;

    void dumpLogs() const;

private:
    std::unique_ptr<class ShaderTaskManagerInternal> mInternal;
    const size_t mThreadCount;
    const ShaderTaskManager::DumpLevel mDumpLevel;
};
} // namespace IG