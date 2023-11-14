#pragma once

#include "Technique.h"

namespace IG {
class AdaptiveEnvPathTechnique : public Technique {
public:
    AdaptiveEnvPathTechnique(SceneObject& obj);
    ~AdaptiveEnvPathTechnique() = default;
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    size_t mMaxDepth;
    size_t mMinDepth;
    std::string mLightSelector;
    bool mEnableNEE;
    float mClamp;
    size_t mNumLearningIterations;
};
} // namespace IG