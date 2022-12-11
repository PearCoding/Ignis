#pragma once

#include "Technique.h"

namespace IG {
class VolumePathTechnique : public Technique {
public:
    VolumePathTechnique(const SceneObject& obj);
    ~VolumePathTechnique() = default;

    bool hasDenoiserSupport() const override { return true; }
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    size_t mMaxDepth;
    std::string mLightSelector;
    float mClamp;
};
} // namespace IG