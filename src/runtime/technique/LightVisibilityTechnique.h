#pragma once

#include "Technique.h"

namespace IG {
class LightVisibilityTechnique : public Technique {
public:
    LightVisibilityTechnique(const std::shared_ptr<SceneObject>& obj);
    ~LightVisibilityTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    std::string mLightSelector;
    float mNoConnectionFactor;

    const std::shared_ptr<SceneObject> mTechnique;
};
} // namespace IG