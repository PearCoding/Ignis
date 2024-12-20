#pragma once

#include "Light.h"

namespace IG {
class SunLight : public Light {
public:
    SunLight(const std::string& name, const std::shared_ptr<SceneObject>& light);

    virtual bool isInfinite() const override { return true; }
    virtual bool isDelta() const override { return true; }
    virtual std::optional<Vector3f> direction() const override { return mDirection; }
    
    virtual float computeFlux(ShadingTree&) const override;
    virtual void serialize(const SerializationInput& input) const override;

private:
    Vector3f mDirection;
    bool mUseRadiance;

    std::shared_ptr<SceneObject> mLight;
};
} // namespace IG