#pragma once

#include "Technique.h"

namespace IG {
class VolumePathTechnique : public Technique {
public:
    VolumePathTechnique(const std::shared_ptr<SceneObject>& obj);
    ~VolumePathTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    std::string mLightSelector;
    bool mEnableNEE;

    const std::shared_ptr<SceneObject> mTechnique;
};
} // namespace IG