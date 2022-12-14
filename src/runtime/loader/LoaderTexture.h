#pragma once

#include "LoaderContext.h"
#include "ShadingTree.h"

namespace IG {
struct LoaderTexture {
    // Returns the filename if available
    static std::filesystem::path getFilename(const SceneObject& obj, const LoaderContext& ctx);
    static std::string generate(const std::string& name, const SceneObject& obj, ShadingTree& tree);
};
} // namespace IG