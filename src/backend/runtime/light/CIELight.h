#pragma once

#include "Light.h"

namespace IG {
enum class CIEType {
    Uniform,
    Cloudy,
    Clear,
    Intermediate
};

class CIELight : public Light {
public:
    CIELight(CIEType classification, const std::string& name, const std::shared_ptr<Parser::Object>& light);

    virtual bool isInfinite() const override { return true; }
    virtual std::optional<Vector3f> direction() const override
    {
        switch (mClassification) {
        default:
            return std::nullopt;
        case CIEType::Clear:
        case CIEType::Intermediate:
            return mSunDirection;
        }
    }

    virtual float computeFlux(const ShadingTree&) const override;
    virtual void serialize(const SerializationInput& input) const override;

private:
    const CIEType mClassification;
    Vector3f mSunDirection;
    bool mHasGround;

    std::shared_ptr<Parser::Object> mLight;
};
} // namespace IG