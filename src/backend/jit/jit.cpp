#include "jit.h"

#include <anydsl_jit.h>

#include <fstream>
#include <sstream>
#include <vector>

extern const char* ig_api[];
extern const char* ig_api_paths[];

// TODO: Really fixed at compile time?
#ifdef IG_DEBUG
constexpr int OPT_LEVEL = 0;
#else
constexpr int OPT_LEVEL = 3;
#endif

namespace IG {
void ig_init_jit(const std::string& driver_path)
{
    anydsl_link(driver_path.c_str());
}

void* ig_compile_source(const std::string& src, const std::string& function, const std::filesystem::path* debug_output)
{
    std::stringstream source;

    for (int i = 0; ig_api[i]; ++i)
        source << ig_api[i];

    source << std::endl;
    source << src;

    const std::string source_str = source.str();
    if (debug_output) {
        std::ofstream stream(debug_output->generic_u8string());
        stream << source_str;
    }

    int ret = anydsl_compile(source_str.c_str(), source_str.size(), OPT_LEVEL);
    if (ret < 0)
        return nullptr;

    return anydsl_lookup_function(ret, function.c_str());
}
} // namespace IG