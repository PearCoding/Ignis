#include <fstream>
#include <iostream>

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

#include "generated_test_interface.h"

int main(int, char**)
{
	// Force flush to zero mode for denormals
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
	_mm_setcsr(_mm_getcsr() | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
#endif

	int err = test_main();

	if (err != 0)
		std::cout << err << " failed tests!" << std::endl;
	else
		std::cout << "All tests passed :)" << std::endl;

	return err;
}
