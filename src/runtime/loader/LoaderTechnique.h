#pragma once

#include "technique/TechniqueInfo.h"

namespace IG {
class LoaderContext;
class LoaderTechnique {
public:
    LoaderTechnique();
    ~LoaderTechnique();

    void setup(const LoaderContext& ctx);
    [[nodiscard]] std::string generate(LoaderContext& ctx);

    [[nodiscard]] inline const TechniqueInfo& info() const { return mInfo; }
    [[nodiscard]] inline bool hasTechnique() const { return mTechnique != nullptr; }

    [[nodiscard]] bool hasDenoiserEnabled() const;

    [[nodiscard]] static std::vector<std::string> getAvailableTypes();

private:
    std::shared_ptr<class Technique> mTechnique;
    TechniqueInfo mInfo;
};
} // namespace IG