#pragma once

#include "Technique.h"

namespace IG {
class AdaptiveEnvPathTechnique : public Technique {
public:
    AdaptiveEnvPathTechnique(const std::shared_ptr<SceneObject>& obj);
    ~AdaptiveEnvPathTechnique() = default;
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    std::string mLightSelector;
    bool mEnableNEE;
    size_t mNumLearningIterations;
    
    const std::shared_ptr<SceneObject> mTechnique;
};
} // namespace IG