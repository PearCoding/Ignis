#include "PointLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "serialization/VectorSerializer.h"

namespace IG {
PointLight::PointLight(const std::string& name, const std::shared_ptr<Parser::Object>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mPosition = light->property("position").getVector3();
}

float PointLight::computeFlux(const ShadingTree& tree) const
{
    return tree.computeNumber("intensity", *mLight, 1) * 4 * Pi;
}

void PointLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    input.Tree.addColor("intensity", *mLight, Vector3f::Ones(), true);

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let light_" << light_id << " = make_point_light(" << input.ID
                 << ", " << LoaderUtils::inlineVector(mPosition)
                 << ", " << input.Tree.getInline("intensity") << ");" << std::endl;

    input.Tree.endClosure();
}

std::optional<std::string> PointLight::getEmbedClass() const
{
    const auto position  = mLight->property("position");
    const auto intensity = mLight->property("intensity");

    const bool simple = (!position.isValid() || position.type() == Parser::PT_VECTOR3)
                        && (!intensity.isValid() || intensity.canBeNumber() || intensity.type() == Parser::PT_VECTOR3);

    return simple ? std::make_optional("SimplePointLight") : std::nullopt;
}


void PointLight::embed(const EmbedInput& input) const
{
    const Vector3f radiance = input.Tree.computeColor("intensity", *mLight, Vector3f::Ones());

    input.Serializer.write(mPosition);             // +3   = 3
    input.Serializer.write((uint32)0 /*Padding*/); // +1   = 4
    input.Serializer.write(radiance);              // +3   = 7
    input.Serializer.write((uint32)0 /*Padding*/); // +1   = 8
}

} // namespace IG