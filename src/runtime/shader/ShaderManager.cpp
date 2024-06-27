#include "ShaderManager.h"
#include "Logger.h"
#include "ShaderProgressBar.h"
#include "ShaderReducer.h"
#include "ShaderTaskManager.h"

namespace IG {
bool ShaderManager::compile(ScriptCompiler* compiler, size_t threads, ShaderDumpVerbosity verbosity )
{
    ShaderTaskManager manager(compiler, threads, verbosity);
    ShaderReducer reducer;

    // Reduce the number of shaders
    for (const auto& p : mEntries) {
        p.second.Compiled->Exec = nullptr; // Reset
        reducer.registerGroupID(p.first, p.second.Source->Exec, p.second.Function);
    }

    const size_t numberOfUniqueEntries = reducer.computeNumberOfUniqueEntries();
    if (numberOfUniqueEntries != reducer.numberOfEntries())
        IG_LOG(L_DEBUG) << "Reduced number of shaders from " << reducer.numberOfEntries() << " to " << numberOfUniqueEntries << " (" << (numberOfUniqueEntries / (float)reducer.numberOfEntries()) << "x)" << std::endl;

    // Start compilation of groups
    const auto& groups = reducer.groups();
    for (auto it = groups.begin(); it != groups.end();) {
        const auto key             = it->first;
        const std::string group_id = reducer.getGroupID(std::get<0>(key), std::get<1>(key));
        manager.add(group_id, it->second, std::get<0>(key), std::get<1>(key));

        // Skip other elements with the same key
        while (++it != groups.end() && it->first == key)
            ;
    }

    manager.finalize();

    if (!IG_LOGGER.isQuiet()) {
        ShaderProgressBar pb(IG_LOGGER.isUsingAnsiTerminal(), 2, numberOfUniqueEntries);
        pb.begin();
        while (!manager.isFinished()) {
            pb.update(manager.numFinishedTasks());
            std::this_thread::yield();
        }
        pb.end();
    }

    if (!manager.waitForFinish()) {
        IG_LOG(L_ERROR) << "Compiling shaders failed" << std::endl;
        manager.dumpLogs();
        return false;
    }

    // Update outputs
    for (const auto& p : mEntries) {
        const auto& entry          = p.second;
        const std::string group_id = reducer.getGroupID(entry.Source->Exec, entry.Function);

        entry.Compiled->Exec          = manager.getResult(group_id);
        entry.Compiled->LocalRegistry = entry.Source->LocalRegistry;
        if (entry.Compiled->Exec == nullptr) {
            if (std::string log = manager.getLog(group_id); !log.empty())
                IG_LOG(L_ERROR) << log << std::endl;
            IG_LOG(L_ERROR) << "Failed to compile '" << p.first << "' in group '" << group_id << "'" << std::endl;
            return false;
        } else {
            if (std::string log = manager.getLog(group_id); !log.empty())
                IG_LOG(L_DEBUG) << log << std::endl;
        }
    }

    return true;
}
} // namespace IG