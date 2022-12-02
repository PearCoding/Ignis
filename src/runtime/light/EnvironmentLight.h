#pragma once

#include "Light.h"

namespace IG {
class EnvironmentLight : public Light {
public:
    EnvironmentLight(const std::string& name, const std::shared_ptr<Parser::Object>& light);

    virtual bool isInfinite() const override { return true; }
    
    virtual float computeFlux(const ShadingTree& tree) const override;
    virtual void serialize(const SerializationInput& input) const override;

private:
    bool mUseCDF;
    bool mUseCompensation;

    std::shared_ptr<Parser::Object> mLight;
};
} // namespace IG