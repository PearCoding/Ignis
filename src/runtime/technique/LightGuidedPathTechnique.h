#pragma once

#include "Technique.h"

namespace IG {
class LightGuidedPathTechnique : public Technique {
public:
    LightGuidedPathTechnique(const std::shared_ptr<SceneObject>& obj);
    ~LightGuidedPathTechnique() = default;
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    std::string mLightSelector;
    bool mEnableNEE;
    bool mMISAOVs;

    const std::shared_ptr<SceneObject> mTechnique;
};
} // namespace IG