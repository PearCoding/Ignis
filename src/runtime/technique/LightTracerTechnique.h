#pragma once

#include "Technique.h"

namespace IG {
class LightTracerTechnique : public Technique {
public:
    LightTracerTechnique(const Parser::Object& obj);
    ~LightTracerTechnique() = default;

    bool hasDenoiserSupport() const override { return true; }
    
    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    size_t mMaxLightDepth;
    std::string mLightSelector;
    float mClamp;
};
} // namespace IG