#include "IG_Config.h"

#if __has_cpp_attribute(__cpp_lib_stacktrace)
#define USE_CPP_STD
#include <iostream>
#include <stacktrace>
#else
#define USE_CPPTRACE
#include <cpptrace/basic.hpp>
#endif

namespace IG {
void internal_assert_handler(const char* file, int line, const char* func, const char* msg)
{
    std::cerr << "[IGNIS] ASSERT | " << file << ":" << line << " " << func << " | " << msg << std::endl;

#if defined(USE_CPP_STD)
    std::cerr << std::stacktrace::current(1 /* Skip this handler */) << std::endl;
#elif defined(USE_CPPTRACE)
    cpptrace::generate_trace(1).print_with_snippets(std::cerr);
#endif

#ifdef IG_DEBUG
    IG_DEBUG_BREAK();
#endif

    std::abort();
}
} // namespace IG