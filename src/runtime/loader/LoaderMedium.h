#pragma once

#include "LoaderContext.h"

namespace IG {
class ShadingTree;
struct LoaderResult;
struct LoaderMedium {
    static std::string generate(ShadingTree& tree);
};
} // namespace IG