#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

#include "ui.h"

#ifndef NDEBUG
#include <fenv.h>
#endif

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

using namespace IG;

int main(int, char**)
{
	int width = 400, height = 400;
	rodent_ui_init(width, height);

	// Force flush to zero mode for denormals
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
	_mm_setcsr(_mm_getcsr() | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
#endif

#if !defined(NDEBUG)
	feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);
#endif

    uint32_t iter;
	Camera camera(Vector3f::Zero(), Vector3f::UnitZ(), Vector3f::UnitY(), 60, width / (float)height);
	bool done = false;
	while (!done) {
		done = rodent_ui_handleinput(iter, camera);
		rodent_ui_update(iter);
	}

	rodent_ui_close();
	return 0;
}
