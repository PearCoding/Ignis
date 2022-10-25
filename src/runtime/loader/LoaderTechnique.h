#pragma once

#include "LoaderContext.h"
#include "TechniqueInfo.h"

namespace IG {
struct LoaderTechnique {
    static std::optional<TechniqueInfo> getInfo(const LoaderContext& ctx);
    static std::string generate(LoaderContext& ctx);

    static std::vector<std::string> getAvailableTypes();
};
} // namespace IG