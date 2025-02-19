#pragma once

#include "Technique.h"

namespace IG {
class VMFGuidedPathTechnique : public Technique {
public:
    VMFGuidedPathTechnique(const std::shared_ptr<SceneObject>& obj);
    ~VMFGuidedPathTechnique() = default;
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    std::string mLightSelector;
    bool mEnableNEE;
    bool mAOVs;

    const std::shared_ptr<SceneObject> mTechnique;
};
} // namespace IG