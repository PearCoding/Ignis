#include "Compiler.h"
#include "Logger.h"
#include "device/IDeviceInterface.h"

#include <anydsl_jit.h>

namespace IG {

static int compileScript(const ICompilerDevice::Settings& settings, const std::string& script)
{
#ifdef IG_DEBUG
    anydsl_set_log_level(settings.Verbose ? 2 /* info */ : 4 /* error */);
#else
    anydsl_set_log_level(settings.Verbose ? 3 /* warn */ : 4 /* error */);
#endif

    return anydsl_compile(script.c_str(), (uint32_t)script.length(), (uint32_t)settings.OptimizationLevel);
}

bool Compiler::compile(const ICompilerDevice::Settings& settings, const std::string& script) const
{
    return compileScript(settings, script) >= 0;
}

void* Compiler::compileAndGet(const ICompilerDevice::Settings& settings, const std::string& script, const std::string& function) const
{
    int ret = compileScript(settings, script);
    if (ret < 0)
        return nullptr;

    void* callback = anydsl_lookup_function(ret, function.c_str());
    if (callback == nullptr)
        IG_LOG(L_ERROR) << "Could not find function '" << function << "' in compiled script" << std::endl;

    return callback;
}

} // namespace IG