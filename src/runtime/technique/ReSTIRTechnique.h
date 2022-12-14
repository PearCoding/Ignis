#pragma once

#include "Technique.h"

namespace IG {
class ReSTIRTechnique : public Technique {
public:
    ReSTIRTechnique(const SceneObject& obj);
    ~ReSTIRTechnique() = default;

    bool hasDenoiserSupport() const override { return true; }
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    size_t mMaxDepth;
    std::string mLightSelector;
    float mClamp;
    bool mMISAOVs;
};
} // namespace IG