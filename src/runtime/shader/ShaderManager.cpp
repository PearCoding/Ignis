#include "ShaderManager.h"
#include "ExternalProcess.h"
#include "Logger.h"
#include "RuntimeInfo.h"
#include "StringUtils.h"

#include <fstream>

#include <tbb/concurrent_queue.h>

namespace IG {
class ShaderManagerInternal {
public:
    struct Work {
        std::string ID;
        std::string Script;
        std::string Function;
    };

    struct Result {
        std::string Log;
        void* Ptr;
    };

    struct RunningProcess {
        ExternalProcess* Proc;
        ShaderManagerInternal::Work Work;
    };

    ScriptCompiler* mInternalCompiler;
    const size_t mThreadCount;

    tbb::concurrent_queue<Work> mWorkQueue;
    std::unordered_map<std::string, Result> mResultMap;
    std::mutex mWorkMutex;
    std::thread mWorkThread;
    std::atomic<bool> mThreadRunning;
    std::atomic<bool> mThreadRequestFinish;

    Path igcPath;
    std::vector<std::string> igcParameters;

    ShaderManagerInternal(ScriptCompiler* compiler, size_t threads)
        : mInternalCompiler(compiler)
        , mThreadCount(threads)
        , mThreadRunning(false)
    {
        igcPath = RuntimeInfo::igcPath();

        igcParameters.push_back("--no-color");
        igcParameters.push_back("--no-logo");

        if (mInternalCompiler->isVerbose())
            igcParameters.push_back("--verbose");

        igcParameters.push_back("-O");
        igcParameters.push_back(std::to_string(mInternalCompiler->optimizationLevel()));
    }

    void run()
    {
        std::vector<RunningProcess> processes(mThreadCount);

        while (mThreadRunning) {
            // Check all processes
            for (auto& p : processes) {
                if (!p.Proc || !p.Proc->isRunning()) {
                    handleExit(p);

                    // Check if there is work todo
                    if (mWorkQueue.try_pop(p.Work))
                        startProcess(p);
                }
            }

            if (mWorkQueue.empty() && mThreadRequestFinish)
                break;

            std::this_thread::yield();
        }

        // Get all the process states
        for (auto& p : processes)
            handleExit(p);
    }

    void start()
    {
        mThreadRunning = true;
        mWorkThread    = std::move(std::thread([this]() { this->run(); }));
    }

    void stop()
    {
        mThreadRunning = false;
        mWorkThread.join();
    }

    void stopWhenFinished()
    {
        mThreadRequestFinish = true;
        mWorkThread.join();
    }

private:
    void startProcess(RunningProcess& proc)
    {
        if (proc.Proc)
            delete proc.Proc;

        IG_LOG(L_DEBUG) << "Starting compilation of '" << proc.Work.ID << "'" << std::endl;

        proc.Proc = new ExternalProcess(igcPath, igcParameters);

        if (!proc.Proc->start()) {
            IG_LOG(L_ERROR) << "Compilation failed with compiler due to a startup error" << std::endl;
            delete proc.Proc;
            proc.Proc = nullptr;
            return;
        }

        proc.Proc->waitForInit();

        if (!proc.Proc->isRunning()) {
            IG_LOG(L_ERROR) << "Compilation failed with compiler early terminating with " << proc.Proc->exitCode() << std::endl;
            IG_LOG(L_DEBUG) << proc.Proc->receiveOnce();
            delete proc.Proc;
            proc.Proc = nullptr;
            return;
        }

        if (!proc.Proc->sendOnce(proc.Work.Script)) {
            IG_LOG(L_ERROR) << "Compilation failed due to failing to establish communication with compiler" << std::endl;
            IG_LOG(L_DEBUG) << proc.Proc->receiveOnce();
            delete proc.Proc;
            proc.Proc = nullptr;
            return;
        }
    }

    void handleExit(RunningProcess& proc)
    {
        if (proc.Work.ID.empty() || !proc.Proc)
            return;

        proc.Proc->waitForFinish();
        int exitCode = proc.Proc->exitCode();

        IG_LOG(L_DEBUG) << "Finished compilation of '" << proc.Work.ID << "' with exit code " << exitCode << std::endl;
        void* ptr = nullptr;
        if (exitCode == EXIT_SUCCESS) {
            // All good -> recompile to get data from cache!
            mWorkMutex.lock();
            ptr = mInternalCompiler->compile(proc.Work.Script, proc.Work.Function);
            mWorkMutex.unlock();
        }

        mWorkMutex.lock();
        mResultMap[proc.Work.ID] = Result{
            .Log = proc.Proc->receiveOnce(),
            .Ptr = ptr
        };
        mWorkMutex.unlock();

        delete proc.Proc;
        proc.Proc = nullptr;
    }
};

ShaderManager::ShaderManager(ScriptCompiler* compiler, size_t threads, DumpLevel dumpLevel)
    : mInternal(new ShaderManagerInternal(compiler, threads))
    , mThreadCount(threads)
    , mDumpLevel(dumpLevel)
{
}

ShaderManager::~ShaderManager()
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

    const std::string full_script = mInternal->mInternalCompiler->prepare(script);

    if (mDumpLevel == DumpLevel::Full)
        dumpShader(whitespace_escaped(id) + ".art", full_script);

    if (mThreadCount == 1) {
        // Fallback internal compilation
        IG_LOG(L_DEBUG) << "Compiling '" << id << "'" << std::endl;
        void* ptr                 = mInternal->mInternalCompiler->compile(full_script, function);
        mInternal->mResultMap[id] = ShaderManagerInternal::Result{
            .Log = {}, // TODO
            .Ptr = ptr,
        };
    } else {
        std::lock_guard<std::mutex> _guard(mInternal->mWorkMutex);
        mInternal->mWorkQueue.push(ShaderManagerInternal::Work{
            .ID       = id,
            .Script   = full_script,
            .Function = function });

        // Start threads
        if (!mInternal->mThreadRunning) {
            mInternal->start();
        }
    }
}

bool ShaderManager::waitForFinish()
{
    if (mThreadCount != 1 && mInternal->mThreadRunning)
        mInternal->stopWhenFinished();

    // Return true if all compilations were successful
    for (const auto& p : mInternal->mResultMap) {
        if (p.second.Ptr == nullptr)
            return false;
    }
    return true;
}

void* ShaderManager::getResult(const std::string& id) const
{
    if (const auto it = mInternal->mResultMap.find(id); it != mInternal->mResultMap.end())
        return it->second.Ptr;
    return nullptr;
}

std::string ShaderManager::getLog(const std::string& id) const
{
    if (const auto it = mInternal->mResultMap.find(id); it != mInternal->mResultMap.end())
        return it->second.Log;
    return {};
}

void ShaderManager::dumpLogs() const
{
    for (const auto& p : mInternal->mResultMap) {
        if (p.second.Log.empty())
            continue;

        IG_LOG(L_INFO) << "Build log '" << p.first << "':" << std::endl
                       << p.second.Log << std::endl;
    }
}
} // namespace IG