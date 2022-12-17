#include "SpotLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "serialization/VectorSerializer.h"

namespace IG {
SpotLight::SpotLight(const std::string& name, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mPosition   = light->property("position").getVector3();
    mDirection  = LoaderUtils::getDirection(*mLight);
    mUsingPower = light->hasProperty("power");
}

static inline float power_factor(float cutoff, float falloff)
{
    return 2 * Pi * (1 - 0.5f * (std::cos(cutoff) + std::cos(falloff)));
}

float SpotLight::computeFlux(const ShadingTree& tree) const
{
    const float cutoff  = tree.computeNumber("cutoff", *mLight, 30) * Deg2Rad;
    const float falloff = tree.computeNumber("falloff", *mLight, 20) * Deg2Rad;
    const float factor  = power_factor(cutoff, falloff);
    if (mUsingPower)
        return tree.computeNumber("power", *mLight, factor);
    else
        return tree.computeNumber("intensity", *mLight, 1) * factor;
}

void SpotLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    if (mUsingPower)
        input.Tree.addColor("power", *mLight, Vector3f::Ones(), true);
    else
        input.Tree.addColor("intensity", *mLight, Vector3f::Ones(), true);

    input.Tree.addNumber("cutoff", *mLight, 30, true);
    input.Tree.addNumber("falloff", *mLight, 20, true);

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let light_" << light_id << " = make_spot_light(" << input.ID
                 << ", " << LoaderUtils::inlineVector(mPosition)
                 << ", " << LoaderUtils::inlineVector(mDirection)
                 << ", rad(" << input.Tree.getInline("cutoff") << ")"
                 << ", rad(" << input.Tree.getInline("falloff") << ")";

    if (mUsingPower)
        input.Stream << ", spot_from_power(" << input.Tree.getInline("power")
                     << ", rad(" << input.Tree.getInline("cutoff") << ")"
                     << ", rad(" << input.Tree.getInline("falloff") << ")));" << std::endl;
    else
        input.Stream << ", " << input.Tree.getInline("intensity") << ");" << std::endl;

    input.Tree.endClosure();
}

std::optional<std::string> SpotLight::getEmbedClass() const
{
    const auto position  = mLight->property("position");
    const auto intensity = mUsingPower ? mLight->property("power") : mLight->property("intensity");
    const auto cutoff    = mLight->property("cutoff");
    const auto falloff   = mLight->property("falloff");

    const bool simple = (!position.isValid() || position.type() == SceneProperty::PT_VECTOR3)
                        && (!intensity.isValid() || intensity.canBeNumber() || intensity.type() == SceneProperty::PT_VECTOR3)
                        && (!cutoff.isValid() || cutoff.canBeNumber())
                        && (!falloff.isValid() || falloff.canBeNumber());

    return simple ? std::make_optional("SimpleSpotLight") : std::nullopt;
}

void SpotLight::embed(const EmbedInput& input) const
{
    const float cutoff       = input.Tree.computeNumber("cutoff", *mLight, 30) * Deg2Rad;
    const float falloff      = input.Tree.computeNumber("falloff", *mLight, 20) * Deg2Rad;
    const float factor       = power_factor(cutoff, falloff);
    const Vector3f intensity = mUsingPower ? input.Tree.computeColor("power", *mLight, Vector3f::Constant(factor)) / factor : input.Tree.computeColor("intensity", *mLight, Vector3f::Ones());

    input.Serializer.write(mPosition);             // +3   = 3
    input.Serializer.write(cutoff);                // +1   = 4
    input.Serializer.write(mDirection);            // +3   = 7
    input.Serializer.write(falloff);               // +1   = 8
    input.Serializer.write(intensity);             // +3   = 11
    input.Serializer.write((uint32)0 /*Padding*/); // +1   = 12
}

} // namespace IG