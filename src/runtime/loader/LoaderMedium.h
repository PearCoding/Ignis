#pragma once

#include "LoaderContext.h"

namespace IG {
class ShadingTree;
struct LoaderMedium {
    static std::string generate(ShadingTree& tree);
};
} // namespace IG