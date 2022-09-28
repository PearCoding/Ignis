#pragma once

#include "Light.h"

namespace IG {
class PerezLight : public Light {
public:
    PerezLight(const std::string& name, const std::shared_ptr<Parser::Object>& light);

    virtual bool isInfinite() const override { return true; }
    virtual std::optional<Vector3f> direction() const override { return mSunDirection; }
    
    virtual float computeFlux(const ShadingTree&) const override;
    virtual void serialize(const SerializationInput& input) const override;

private:
    Vector3f mSunDirection;

    std::shared_ptr<Parser::Object> mLight;
};
} // namespace IG