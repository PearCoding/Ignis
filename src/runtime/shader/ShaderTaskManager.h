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
    /// After this no tasks are expected to arrive
    void finalize(); 

    [[nodiscard]] bool isFinished();
    [[nodiscard]] bool hasError() const;
    bool waitForFinish();

    [[nodiscard]] size_t numFinishedTasks() const;

    [[nodiscard]] void* getResult(const std::string& id) const;
    [[nodiscard]] std::string getLog(const std::string& id) const;

    void dumpLogs() const;

private:
    std::unique_ptr<class ShaderTaskManagerInternal> mInternal;
    const size_t mThreadCount;
    const ShaderTaskManager::DumpLevel mDumpLevel;
};
} // namespace IG