#pragma once

#include "Technique.h"

namespace IG {
class PathTechnique : public Technique {
public:
    PathTechnique(SceneObject& obj);
    ~PathTechnique() = default;
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    size_t mMaxDepth;
    size_t mMinDepth;
    std::string mLightSelector;
    float mClamp;
    bool mEnableNEE;
    bool mMISAOVs;
};
} // namespace IG