#include "SunLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

#include "Logger.h"

namespace IG {
SunLight::SunLight(const std::string& name, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mDirection = LoaderUtils::getDirection(*light);
    mUseRadius = light->hasProperty("radius");

    IG_LOG(L_DEBUG) << "Light '" << name << "' has direction " << FormatVector(mDirection) << std::endl;
}

float SunLight::computeFlux(ShadingTree& tree) const
{
    IG_UNUSED(tree);
    // TODO: This is only an approximation

    constexpr float radius = 5.2f;
    return tree.computeNumber("irradiance", *mLight, 1.0f).Value * Pi * radius * radius;
}

void SunLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addColor("irradiance", *mLight, Vector3f::Ones());
    if (mUseRadius)
        input.Tree.addNumber("radius", *mLight, 5.2f);
    else
        input.Tree.addNumber("angle", *mLight, 11.4f);

    if (mLight->hasProperty("direction"))
        input.Tree.addVector("direction", *mLight, Vector3f::UnitY());

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let light_" << light_id << " = make_sun_light(" << input.ID;

    if (mLight->hasProperty("direction"))
        input.Stream << ", " << input.Tree.getInline("direction");
    else
        input.Stream << ", " << LoaderUtils::inlineVector(mDirection);

    input.Stream << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context());

    if (mUseRadius)
        input.Stream << ", sun_cos_angle_from_radius(" << input.Tree.getInline("radius") << ")";
    else
        input.Stream << ", math_builtins::cos(rad(" << input.Tree.getInline("angle") << "/2))";

    input.Stream << ", " << input.Tree.getInline("irradiance") << ", false);" << std::endl;

    input.Tree.endClosure();
}

} // namespace IG