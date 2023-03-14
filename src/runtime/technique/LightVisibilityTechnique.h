#pragma once

#include "Technique.h"

namespace IG {
class LightVisibilityTechnique : public Technique {
public:
    LightVisibilityTechnique(SceneObject& obj);
    ~LightVisibilityTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    size_t mMaxDepth;
    std::string mLightSelector;
    float mNoConnectionFactor;
};
} // namespace IG