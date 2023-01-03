#pragma once

#include "IG_Config.h"

namespace IG {
class BSDF;
class LoaderContext;
class ShadingTree;
class LoaderBSDF {
public:
    void prepare(const LoaderContext& ctx);

    [[nodiscard]] std::string generate(const std::string& name, ShadingTree& tree);

    [[nodiscard]] inline size_t bsdfCount() const { return mAvailableBSDFs.size(); }

private:
    std::unordered_map<std::string, std::shared_ptr<BSDF>> mAvailableBSDFs; // All available bsdfs, not necessarily the ones loaded at the end!
};
} // namespace IG