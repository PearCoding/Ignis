#include "Compiler.h"
#include "Logger.h"
#include "RuntimeInfo.h"

#include <anydsl_jit.h>
#include <anydsl_runtime.hpp>

namespace IG {
void* Compiler::compile(const ICompilerDevice::Settings& settings, const std::string& script, const std::string& function) const
{
    static std::once_flag once;
    std::call_once(once, []() {
        const auto module_path = RuntimeInfo::modulePath();
        if (!module_path.empty()) {
            IG_LOG(L_DEBUG) << "Loading symbolic module " << module_path << std::endl;
            anydsl_link(module_path.generic_string().c_str());
        }

        const auto cache_dir = RuntimeInfo::cacheDirectory();
        anydsl_set_cache_directory(cache_dir.generic_string().c_str());
    });

#ifdef IG_DEBUG
    anydsl_set_log_level(settings.Verbose ? 2 /* info */ : 4 /* error */);
#else
    anydsl_set_log_level(settings.Verbose ? 3 /* warn */ : 4 /* error */);
#endif

    const int ret = anydsl_compile(script.c_str(), (uint32_t)script.length(), (uint32_t)settings.OptimizationLevel);
    if (ret < 0)
        return nullptr;

    void* callback = anydsl_lookup_function(ret, function.c_str());
    if (callback == nullptr)
        IG_LOG(L_ERROR) << "Could not find function '" << function << "' in compiled script" << std::endl;

    return callback;
}

} // namespace IG