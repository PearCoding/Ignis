#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderBSDF {
    static std::string generate(const std::string& name, const LoaderContext& ctx);
};
} // namespace IG