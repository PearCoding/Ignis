#pragma once

#include "Light.h"

namespace IG {
class SkyLight : public Light {
public:
    SkyLight(const std::string& name, const std::shared_ptr<Parser::Object>& light);

    virtual bool isInfinite() const override { return true; }
    virtual std::optional<Vector3f> direction() const override { return mSunDirection; }
    
    virtual float computeFlux(const ShadingTree& tree) const override;
    virtual void serialize(const SerializationInput& input) const override;

private:
    Vector3f mSunDirection;
    float mTotalFlux;

    std::shared_ptr<Parser::Object> mLight;
};
} // namespace IG