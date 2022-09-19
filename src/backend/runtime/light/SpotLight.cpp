#include "SpotLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "serialization/VectorSerializer.h"

namespace IG {
SpotLight::SpotLight(const std::string& name, const std::shared_ptr<Parser::Object>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mPosition  = light->property("position").getVector3();
    mDirection = LoaderUtils::getDirection(*mLight);
}

float SpotLight::computeFlux(const ShadingTree& tree) const
{
    const float power   = tree.computeNumber("intensity", *mLight, 1);
    const float cutoff  = tree.computeNumber("cutoff", *mLight, 30) * Deg2Rad;
    const float falloff = tree.computeNumber("falloff", *mLight, 20) * Deg2Rad;
    return power * 2 * Pi * (1 - 0.5f * (std::cos(cutoff) + std::cos(falloff)));
}

void SpotLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addColor("intensity", *mLight, Vector3f::Ones(), true);
    input.Tree.addNumber("cutoff", *mLight, 30, true);
    input.Tree.addNumber("falloff", *mLight, 20, true);

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let light_" << light_id << " = make_spot_light(" << input.ID
                 << ", " << LoaderUtils::inlineVector(mPosition)
                 << ", " << LoaderUtils::inlineVector(mDirection)
                 << ", rad(" << input.Tree.getInline("cutoff") << ")"
                 << ", rad(" << input.Tree.getInline("falloff") << ")"
                 << ", " << input.Tree.getInline("intensity") << ");" << std::endl;

    input.Tree.endClosure();
}

std::optional<std::string> SpotLight::getEmbedClass() const
{
    const auto position  = mLight->property("position");
    const auto intensity = mLight->property("intensity");
    const auto cutoff    = mLight->property("cutoff");
    const auto falloff   = mLight->property("falloff");

    const bool simple = (!position.isValid() || position.type() == Parser::PT_VECTOR3)
                        && (!intensity.isValid() || intensity.canBeNumber() || intensity.type() == Parser::PT_VECTOR3)
                        && (!cutoff.isValid() || cutoff.canBeNumber())
                        && (!falloff.isValid() || falloff.canBeNumber());

    return simple ? std::make_optional("SimpleSpotLight") : std::nullopt;
}

void SpotLight::embed(const EmbedInput& input) const
{
    const Vector3f radiance = input.Tree.computeColor("intensity", *mLight, Vector3f::Ones());
    const float cutoff      = input.Tree.computeNumber("cutoff", *mLight, 30) * Deg2Rad;
    const float falloff     = input.Tree.computeNumber("falloff", *mLight, 20) * Deg2Rad;

    input.Serializer.write(mPosition);             // +3   = 3
    input.Serializer.write(cutoff);                // +1   = 4
    input.Serializer.write(mDirection);            // +3   = 7
    input.Serializer.write(falloff);               // +1   = 8
    input.Serializer.write(radiance);              // +3   = 11
    input.Serializer.write((uint32)0 /*Padding*/); // +1   = 12
}

} // namespace IG