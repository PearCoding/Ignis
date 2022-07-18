#pragma once

#include "Light.h"

namespace IG {
class LoaderContext;

class AreaLight : public Light {
public:
    AreaLight(const std::string& name, const LoaderContext& ctx, const std::shared_ptr<Parser::Object>& light);

    virtual std::optional<Vector3f> position() const override { return mPosition; }
    virtual std::optional<Vector3f> direction() const override { return mOptimized ? std::make_optional(mDirection) : std::nullopt; }
    virtual std::optional<std::string> entity() const override { return mEntity; }
    virtual float computeFlux(const ShadingTree&) const override;

    virtual void serialize(const SerializationInput& input) const override;

    virtual std::optional<std::string> getEmbedClass() const override;
    virtual void embed(const EmbedInput& input) const override;

private:
    Vector3f mPosition;
    Vector3f mDirection; // ~ Normal
    float mArea;
    std::string mEntity;

    bool mOptimized;
    std::shared_ptr<Parser::Object> mLight;
};
} // namespace IG