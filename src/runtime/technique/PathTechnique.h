#pragma once

#include "Technique.h"

namespace IG {
class PathTechnique : public Technique {
public:
    PathTechnique(const std::shared_ptr<SceneObject>& obj);
    ~PathTechnique() = default;
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    std::string mLightSelector;
    bool mEnableNEE;
    bool mMISAOVs;

    const std::shared_ptr<SceneObject> mTechnique;
};
} // namespace IG