#pragma once

#include "Technique.h"

namespace IG {
class SunGuidedPathTechnique : public Technique {
public:
    SunGuidedPathTechnique(const std::shared_ptr<SceneObject>& technique);
    ~SunGuidedPathTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    size_t mMaxDepth;
    size_t mMinDepth;
    std::string mLightSelector;
    float mClamp;
    bool mEnableNEE;
    bool mMISAOVs;

    float mDefensive;
    Vector3f mSunDirection;

    std::shared_ptr<SceneObject> mTechnique;
};
} // namespace IG