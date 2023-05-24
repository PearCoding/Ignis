#pragma once

#include "IG_Config.h"

namespace IG {
class Pattern;
class LoaderContext;
class ShadingTree;
class LoaderTexture {
public:
    void prepare(const LoaderContext& ctx);

    [[nodiscard]] std::string generate(const std::string& name, ShadingTree& tree);
    [[nodiscard]] std::pair<size_t, size_t> computeResolution(const std::string& name, ShadingTree& tree);

    [[nodiscard]] inline size_t textureCount() const { return mAvailablePatterns.size(); }

private:
    std::unordered_map<std::string, std::shared_ptr<Pattern>> mAvailablePatterns; // All available patterns, not necessarily the ones loaded at the end!
};
} // namespace IG