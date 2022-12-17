#include "PointLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "serialization/VectorSerializer.h"

namespace IG {
PointLight::PointLight(const std::string& name, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mPosition   = light->property("position").getVector3();
    mUsingPower = light->hasProperty("power");
}

constexpr float SR = 4 * Pi;
float PointLight::computeFlux(const ShadingTree& tree) const
{
    if (mUsingPower)
        return tree.computeNumber("power", *mLight, SR);
    else
        return tree.computeNumber("intensity", *mLight, 1) * SR;
}

void PointLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    if (mUsingPower)
        input.Tree.addColor("power", *mLight, Vector3f::Constant(SR), true);
    else
        input.Tree.addColor("intensity", *mLight, Vector3f::Ones(), true);

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let light_" << light_id << " = make_point_light(" << input.ID
                 << ", " << LoaderUtils::inlineVector(mPosition);

    if (mUsingPower)
        input.Stream << ", color_mulf(" << input.Tree.getInline("power") << ", 1 / (4*flt_pi)));" << std::endl;
    else
        input.Stream << ", " << input.Tree.getInline("intensity") << ");" << std::endl;

    input.Tree.endClosure();
}

std::optional<std::string> PointLight::getEmbedClass() const
{
    const auto position  = mLight->property("position");
    const auto intensity = mUsingPower ? mLight->property("power") : mLight->property("intensity");

    const bool simple = (!position.isValid() || position.type() == SceneProperty::PT_VECTOR3)
                        && (!intensity.isValid() || intensity.canBeNumber() || intensity.type() == SceneProperty::PT_VECTOR3);

    return simple ? std::make_optional("SimplePointLight") : std::nullopt;
}

void PointLight::embed(const EmbedInput& input) const
{
    const Vector3f intensity = mUsingPower ? input.Tree.computeColor("power", *mLight, Vector3f::Constant(SR)) / SR : input.Tree.computeColor("intensity", *mLight, Vector3f::Ones());

    input.Serializer.write(mPosition);             // +3   = 3
    input.Serializer.write((uint32)0 /*Padding*/); // +1   = 4
    input.Serializer.write(intensity);             // +3   = 7
    input.Serializer.write((uint32)0 /*Padding*/); // +1   = 8
}

} // namespace IG