#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif

#include "device/Target.h"

#include "generated_test_interface.h"

int main(int argc, char** argv)
{
    // Force flush to zero mode for denormals
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
    _mm_setcsr(_mm_getcsr() | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
#endif

    bool no_gpu = !IG::Target::pickBest().isGPU();
    if (argc > 1 && std::strcmp(argv[1], "--no-gpu") == 0)
        no_gpu = true;

    int err = test_main(no_gpu);

    if (err != 0)
        std::cout << err << " failed tests!" << std::endl;
    else
        std::cout << "All tests passed :)" << std::endl;

    return err;
}
