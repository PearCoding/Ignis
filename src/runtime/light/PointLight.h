#pragma once

#include "Light.h"

namespace IG {
class PointLight : public Light {
public:
    PointLight(const std::string& name, const std::shared_ptr<SceneObject>& light);

    virtual bool isDelta() const override { return true; }
    virtual std::optional<Vector3f> position() const override { return mPosition; }
    virtual float computeFlux(const ShadingTree&) const override;

    virtual void serialize(const SerializationInput& input) const override;

    virtual std::optional<std::string> getEmbedClass() const override;
    virtual void embed(const EmbedInput& input) const override;

private:
    Vector3f mPosition;

    std::shared_ptr<SceneObject> mLight;
};
} // namespace IG