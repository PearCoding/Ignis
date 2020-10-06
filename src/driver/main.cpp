#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

#include "ui.h"

#include "Color.h"
#include "bvh/BVH.h"
#include "math/BoundingBox.h"
#include "math/Triangle.h"
#include "Buffer.h"

#ifndef NDEBUG
#include <fenv.h>
#endif

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

using namespace IG;

int main(int, char**)
{
	int width = 800, height = 600;
	UI::init(width, height);

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
		done = UI::handleInput(iter, camera);
		UI::update(iter);
	}

	UI::close();
	return 0;
}
