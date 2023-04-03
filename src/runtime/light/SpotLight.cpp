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

void SpotLight::precompute(ShadingTree& tree)
{
    const auto cutoff  = tree.computeNumber("cutoff", *mLight, 30);
    const auto falloff = tree.computeNumber("falloff", *mLight, 20);
    const float factor = power_factor(cutoff.Value * Deg2Rad, falloff.Value * Deg2Rad);
    const auto output  = tree.computeColor(mUsingPower ? "power" : "intensity", *mLight, Vector3f::Constant(mUsingPower ? factor : 1.0f));

    mColor_Cache = output.Value;
    if (!mUsingPower)
        mColor_Cache *= factor;
    mIsSimple = output.WasConstant && cutoff.WasConstant && falloff.WasConstant;
}

float SpotLight::computeFlux(ShadingTree& tree) const
{
    IG_UNUSED(tree);
    return mColor_Cache.mean();
}

void SpotLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    if (mUsingPower)
        input.Tree.addColor("power", *mLight, Vector3f::Ones());
    else
        input.Tree.addColor("intensity", *mLight, Vector3f::Ones());

    input.Tree.addNumber("cutoff", *mLight, 30.0f);
    input.Tree.addNumber("falloff", *mLight, 20.0f);

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
    const auto position = mLight->property("position");

    const bool simple = (!position.isValid() || position.type() == SceneProperty::PT_VECTOR3)
                        && mIsSimple;

    return simple ? std::make_optional("SimpleSpotLight") : std::nullopt;
}

void SpotLight::embed(const EmbedInput& input) const
{
    const auto cutoff        = input.Tree.computeNumber("cutoff", *mLight, 30);
    const auto falloff       = input.Tree.computeNumber("falloff", *mLight, 20);
    const float factor       = power_factor(cutoff.Value * Deg2Rad, falloff.Value * Deg2Rad);
    const Vector3f intensity = mColor_Cache / factor;

    input.Serializer.write(mPosition);               // +3   = 3
    input.Serializer.write(cutoff.Value * Deg2Rad);  // +1   = 4
    input.Serializer.write(mDirection);              // +3   = 7
    input.Serializer.write(falloff.Value * Deg2Rad); // +1   = 8
    input.Serializer.write(intensity);               // +3   = 11
    input.Serializer.write((uint32)0 /*Padding*/);   // +1   = 12
}

} // namespace IG