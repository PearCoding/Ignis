#pragma once

#include "Technique.h"

namespace IG {
class PhotonMappingTechnique : public Technique {
public:
    PhotonMappingTechnique(SceneObject& obj);
    ~PhotonMappingTechnique() = default;
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    size_t mPhotonCount;
    size_t mMaxCameraDepth;
    size_t mMinCameraDepth;
    size_t mMaxLightDepth;
    std::string mLightSelector;
    float mMergeRadius;
    float mClamp;
    bool mAOV;
};
} // namespace IG