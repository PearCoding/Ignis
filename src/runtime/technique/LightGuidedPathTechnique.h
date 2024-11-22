#pragma once

#include "Technique.h"

namespace IG {
class LightGuidedPathTechnique : public Technique {
public:
    LightGuidedPathTechnique(SceneObject& obj);
    ~LightGuidedPathTechnique() = default;
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    size_t mMaxDepth;
    size_t mMinDepth;
    std::string mLightSelector;
    float mClamp;
    bool mEnableNEE;
    bool mMISAOVs;

    std::string mLight;
    float mDefensive;
};
} // namespace IG