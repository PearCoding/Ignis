#pragma once

#include "technique/TechniqueInfo.h"

namespace IG {
class LoaderContext;
class ShadingTree;

class LoaderTechnique {
public:
    LoaderTechnique();
    ~LoaderTechnique();

    void setup(const LoaderContext& ctx);

    /// Will generate 'full_technique' identifier
    [[nodiscard]] std::string generate(ShadingTree& tree);

    [[nodiscard]] inline const TechniqueInfo& info() const { return mInfo; }
    [[nodiscard]] inline bool hasTechnique() const { return mTechnique != nullptr; }

    [[nodiscard]] static std::vector<std::string> getAvailableTypes();

private:
    std::shared_ptr<class Technique> mTechnique;
    TechniqueInfo mInfo;
};
} // namespace IG