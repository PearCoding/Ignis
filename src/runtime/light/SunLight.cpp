#include "SunLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "skysun/Illuminance.h"

#include "Logger.h"

namespace IG {
SunLight::SunLight(const std::string& name, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mDirection   = LoaderUtils::getDirection(*light);
    mUseRadiance = light->hasProperty("radiance");

    IG_LOG(L_DEBUG) << "Light '" << name << "' has direction " << FormatVector(mDirection) << std::endl;
}

float SunLight::computeFlux(ShadingTree& tree) const
{
    IG_UNUSED(tree);
    // TODO: This is only an approximation

    return tree.computeNumber("irradiance", *mLight, 1.0f).Value * Pi * (FltSunRadiusDegree * Deg2Rad / 2) * (FltSunRadiusDegree * Deg2Rad / 2);
}

void SunLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    if (mUseRadiance)
        input.Tree.addColor("radiance", *mLight, Vector3f::Ones());
    else
        input.Tree.addColor("irradiance", *mLight, Vector3f::Ones());

    input.Tree.addNumber("angle", *mLight, FltSunRadiusDegree);

    if (mLight->hasProperty("direction"))
        input.Tree.addVector("direction", *mLight, Vector3f::UnitY());

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let light_" << light_id << " = make_sun_light(" << input.ID;

    if (mLight->hasProperty("direction"))
        input.Stream << ", vec3_normalize(" << input.Tree.getInline("direction") << ")";
    else
        input.Stream << ", vec3_normalize(" << LoaderUtils::inlineVector(mDirection) << ")";

    input.Stream << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
                 << ", math_builtins::cos(rad(" << input.Tree.getInline("angle") << "/2))";

    if (mUseRadiance) {
        input.Stream << ", " << input.Tree.getInline("radiance");
    } else {
        input.Stream << ", color_mulf(" << input.Tree.getInline("irradiance")
                     << ", 1 / sun_area_from_srad(rad(" << input.Tree.getInline("angle") << "/2)))";
    }

    input.Stream << ", false);" << std::endl;

    input.Tree.endClosure();
}

} // namespace IG