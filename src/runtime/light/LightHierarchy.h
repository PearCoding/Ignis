#pragma once

#include "IG_Config.h"

namespace IG {
class Light;
class ShadingTree;
class LightHierarchy {
public:
    static std::filesystem::path setup(const std::vector<std::shared_ptr<Light>>& lights, ShadingTree& tree);
};
} // namespace IG