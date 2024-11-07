#pragma once

#include "Light.h"
#include "skysun/SunLocation.h"

namespace IG {
class PerezLight : public Light {
public:
    PerezLight(const std::string& name, const std::shared_ptr<SceneObject>& light);

    virtual bool isInfinite() const override { return true; }
    virtual std::optional<Vector3f> direction() const override { return mSunDirection; }

    virtual float computeFlux(ShadingTree&) const override;
    virtual void serialize(const SerializationInput& input) const override;

private:
    enum class OutputMode {
        VisibleRadiance,
        SolarRadiance,
        Luminance
    };

    Vector3f mSunDirection;
    TimePoint mTimePoint;
    bool mHasGround;
    bool mHasSun;
    OutputMode mOutputMode;

    std::shared_ptr<SceneObject> mLight;
};
} // namespace IG