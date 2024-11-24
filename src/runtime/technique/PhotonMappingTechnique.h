#pragma once

#include "Technique.h"

namespace IG {
class PhotonMappingTechnique : public Technique {
public:
    PhotonMappingTechnique(const std::shared_ptr<SceneObject>& obj);
    ~PhotonMappingTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    size_t mPhotonCount;
    std::string mLightSelector;
    bool mAOV;

    const std::shared_ptr<SceneObject> mTechnique;
};
} // namespace IG