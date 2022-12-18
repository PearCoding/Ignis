#pragma once

#include "Light.h"

namespace IG {
class EnvironmentLight : public Light {
public:
    EnvironmentLight(const std::string& name, const std::shared_ptr<SceneObject>& light);

    virtual bool isInfinite() const override { return true; }
    
    virtual float computeFlux(ShadingTree& tree) const override;
    virtual void serialize(const SerializationInput& input) const override;

private:
    bool mUseCDF;
    bool mUseCompensation;

    std::shared_ptr<SceneObject> mLight;
};
} // namespace IG