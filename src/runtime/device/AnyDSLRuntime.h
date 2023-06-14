#pragma once

#include "IG_Config.h"
#include "anydsl_runtime.hpp"

namespace IG {
void anydslInit();
void anydslInspect();

bool checkAnyDSLResult(AnyDSLResult result, const char* func, const char* file, int line);

#define IG_CHECK_ANYDSL(res) \
    IG::checkAnyDSLResult((res), IG_FUNCTION_NAME, __FILE__, __LINE__)
}; // namespace IG