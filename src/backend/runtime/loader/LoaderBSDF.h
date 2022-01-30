#pragma once

#include "IG_Config.h"

namespace IG {
class ShadingTree;
struct LoaderResult;
struct LoaderBSDF {
    static std::string generate(const std::string& name, ShadingTree& ctx);
};
} // namespace IG