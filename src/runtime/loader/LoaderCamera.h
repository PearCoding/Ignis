#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderCamera {
    static std::string generate(const LoaderContext& ctx);
    static void setupInitialOrientation(LoaderContext& ctx);

    static std::vector<std::string> getAvailableTypes();
};
} // namespace IG