#pragma once

#include "LoaderContext.h"
#include "ShadingTree.h"

namespace IG {
struct LoaderTexture {
    // Returns the filename if available
    static std::string getFilename(const Parser::Object& obj, const LoaderContext& ctx);
    static std::string generate(const std::string& name, const Parser::Object& obj, ShadingTree& tree);
};
} // namespace IG