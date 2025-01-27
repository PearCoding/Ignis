#include "DeviceUtils.h"

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#ifdef IG_CC_MSC
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif

namespace IG {
[[maybe_unused]] thread_local unsigned int stPrevMathMode = 0;
void enableFastMathModeForThread()
{
    // Force flush to zero mode for denormals
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
    stPrevMathMode = _mm_getcsr();
    _mm_setcsr(stPrevMathMode | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
#endif
}

void disableFastMathModeForThread()
{
    // Reset mode
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
    _mm_setcsr(stPrevMathMode);
#endif
}
} // namespace IG