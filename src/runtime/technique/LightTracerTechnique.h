#pragma once

#include "Technique.h"

namespace IG {
class LightTracerTechnique : public Technique {
public:
    LightTracerTechnique(const std::shared_ptr<SceneObject>& obj);
    ~LightTracerTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    std::string mLightSelector;

    const std::shared_ptr<SceneObject> mTechnique;
};
} // namespace IG