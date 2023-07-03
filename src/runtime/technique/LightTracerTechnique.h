#pragma once

#include "Technique.h"

namespace IG {
class LightTracerTechnique : public Technique {
public:
    LightTracerTechnique(SceneObject& obj);
    ~LightTracerTechnique() = default;
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    size_t mMaxLightDepth;
    size_t mMinLightDepth;
    std::string mLightSelector;
    float mClamp;
};
} // namespace IG