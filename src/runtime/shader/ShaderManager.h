#pragma once

#include "ScriptCompiler.h"

namespace IG {
class ShaderManager {
public:
    enum DumpLevel {
        None,
        Light,
        Full
    };

    ShaderManager(ScriptCompiler* compiler, size_t threads, DumpLevel dumpLevel = DumpLevel::None);

    void add(const std::string& id, const std::string& script, const std::string& function);

    bool waitForFinish();

    void* getResult(const std::string& id) const;
    std::string getLog(const std::string& id) const;

private:
    struct Work {
        std::string ID;
        std::string Script;
        std::string Function;
        bool Check;
    };

    struct Result {
        std::string Log;
        void* Ptr;
    };

    ScriptCompiler* mInternalCompiler;
    const size_t mThreads;
    const DumpLevel mDumpLevel;

    std::vector<Work> mWorkQueue;
    std::unordered_map<std::string, Result> mResultMap;
    std::mutex mWorkMutex;
};
} // namespace IG