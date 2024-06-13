#include "ShaderTaskManager.h"
#include "ExternalProcess.h"
#include "Logger.h"
#include "RuntimeInfo.h"
#include "StringUtils.h"

#include <fstream>

#include <tbb/concurrent_queue.h>

namespace IG {
static inline void dumpShader(const Path& filename, const std::string& shader)
{
    try {
        std::ofstream stream(filename);
        stream << shader;
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        // Ignore else
    }
}

class ShaderTaskManagerInternal {
public:
    struct Work {
        std::string ID;
        std::string Name;
        std::string Script;
        std::string Function;

        inline std::string reasonableID() const { return Name + "_" + ID; }
    };

    struct Result {
        std::string Log;
        void* Ptr;
    };

    struct RunningProcess {
        ExternalProcess* Proc;
        std::chrono::time_point<std::chrono::steady_clock> Start;
        ShaderTaskManagerInternal::Work Work;
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

    ShaderTaskManagerInternal(ScriptCompiler* compiler, size_t threads)
        : mInternalCompiler(compiler)
        , mThreadCount(threads)
        , mThreadRunning(false)
        , mThreadRequestFinish(false)
    {
        igcPath = RuntimeInfo::igcPath();

        igcParameters.push_back("--no-color");
        igcParameters.push_back("--no-logo");

        if (mInternalCompiler->isVerbose()) {
            igcParameters.push_back("--verbose");
        } else {
            igcParameters.push_back("--log-level");
            igcParameters.push_back("warning");
        }

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
                    if (mWorkQueue.try_pop(p.Work)) {
                        startProcess(p);
                        break; // Start the parent loop anew
                    }
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
        if (!mThreadRunning.exchange(true))
            mWorkThread = std::move(std::thread([this]() { this->run(); }));
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

        IG_LOG(L_DEBUG) << "Starting compilation of '" << proc.Work.Name << "' for group '" << proc.Work.ID << "'" << std::endl;

        const Path logFile = std::filesystem::temp_directory_path() / "Ignis" / (whitespace_escaped(proc.Work.reasonableID()) + ".log");

        proc.Start = std::chrono::steady_clock::now();
        proc.Proc  = new ExternalProcess(proc.Work.Name + "_" + proc.Work.ID, igcPath, igcParameters, logFile);

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

        const auto dur = std::chrono::steady_clock::now() - proc.Start;

        void* ptr = nullptr;
        if (exitCode == EXIT_SUCCESS) {
            IG_LOG(L_DEBUG) << "Finished compilation of '" << proc.Work.Name << "' for group '" << proc.Work.ID << "' with exit code " << exitCode << " (" << dur << ")" << std::endl;

            // All good -> recompile to get data from cache!
            ptr = mInternalCompiler->compile(proc.Work.Script, proc.Work.Function);
        } else {
            // Dump shader into tmp folder
            const Path tmpFile = std::filesystem::temp_directory_path() / "Ignis" / (whitespace_escaped(proc.Work.reasonableID()) + ".art");
            dumpShader(tmpFile, proc.Work.Script);

            IG_LOG(L_ERROR) << "Finished compilation of '" << proc.Work.Name << "' for group '" << proc.Work.ID << "' with exit code " << exitCode << " (" << dur << ")." << std::endl
                            << "Dump of shader is available at " << tmpFile << std::endl;
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

ShaderTaskManager::ShaderTaskManager(ScriptCompiler* compiler, size_t threads, DumpLevel dumpLevel)
    : mInternal(new ShaderTaskManagerInternal(compiler, threads))
    , mThreadCount(threads)
    , mDumpLevel(dumpLevel)
{
    std::filesystem::create_directories(std::filesystem::temp_directory_path() / "Ignis");
}

ShaderTaskManager::~ShaderTaskManager()
{
}

void ShaderTaskManager::add(const std::string& id, const std::string& name, const std::string& script, const std::string& function)
{
    if (mDumpLevel == DumpLevel::Light)
        dumpShader(whitespace_escaped(name) + ".art", script);

    const std::string full_script = mInternal->mInternalCompiler->prepare(script);

    if (mDumpLevel == DumpLevel::Full)
        dumpShader(whitespace_escaped(name) + ".art", full_script);

    if (mThreadCount == 1) {
        // Fallback internal compilation
        IG_LOG(L_DEBUG) << "Compiling '" << name << "' for group '" << id << "'" << std::endl;
        void* ptr                 = mInternal->mInternalCompiler->compile(full_script, function);
        mInternal->mResultMap[id] = ShaderTaskManagerInternal::Result{
            .Log = {}, // TODO
            .Ptr = ptr,
        };
    } else {
        mInternal->mWorkQueue.push(ShaderTaskManagerInternal::Work{
            .ID       = id,
            .Name     = name,
            .Script   = full_script,
            .Function = function });

        // Start threads
        if (!mInternal->mThreadRunning)
            mInternal->start();
    }
}

bool ShaderTaskManager::waitForFinish()
{
    if (mThreadCount != 1 && mInternal->mThreadRunning)
        mInternal->stopWhenFinished();

    // Return true if all compilations were successful
    std::lock_guard<std::mutex> _guard(mInternal->mWorkMutex);
    for (const auto& p : mInternal->mResultMap) {
        if (p.second.Ptr == nullptr)
            return false;
    }
    return true;
}

void* ShaderTaskManager::getResult(const std::string& id) const
{
    std::lock_guard<std::mutex> _guard(mInternal->mWorkMutex);
    if (const auto it = mInternal->mResultMap.find(id); it != mInternal->mResultMap.end())
        return it->second.Ptr;
    return nullptr;
}

std::string ShaderTaskManager::getLog(const std::string& id) const
{
    std::lock_guard<std::mutex> _guard(mInternal->mWorkMutex);
    if (const auto it = mInternal->mResultMap.find(id); it != mInternal->mResultMap.end())
        return it->second.Log;
    return {};
}

void ShaderTaskManager::dumpLogs() const
{
    std::lock_guard<std::mutex> _guard(mInternal->mWorkMutex);
    for (const auto& p : mInternal->mResultMap) {
        if (p.second.Log.empty())
            continue;

        IG_LOG(L_INFO) << "Build log '" << p.first << "':" << std::endl
                       << p.second.Log << std::endl;
    }
}
} // namespace IG