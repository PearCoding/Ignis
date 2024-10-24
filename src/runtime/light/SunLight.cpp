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
    mUseRadius   = light->hasProperty("radius");
    mUseRadiance = light->hasProperty("radiance");

    IG_LOG(L_DEBUG) << "Light '" << name << "' has direction " << FormatVector(mDirection) << std::endl;
}

float SunLight::computeFlux(ShadingTree& tree) const
{
    IG_UNUSED(tree);
    // TODO: This is only an approximation

    constexpr float radius = 0.01f;
    return tree.computeNumber("irradiance", *mLight, 1.0f).Value * Pi * radius * radius;
}

void SunLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    if (mUseRadiance)
        input.Tree.addColor("radiance", *mLight, Vector3f::Ones());
    else
        input.Tree.addColor("irradiance", *mLight, Vector3f::Ones());

    if (mUseRadius)
        input.Tree.addNumber("radius", *mLight, 0.01f);
    else
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

    input.Stream << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context());

    if (mUseRadius)
        input.Stream << ", sun_cos_angle_from_radius(" << input.Tree.getInline("radius") << ")";
    else
        input.Stream << ", math_builtins::cos(rad(" << input.Tree.getInline("angle") << "/2))";

    if(mUseRadiance) {
        input.Stream << ", " << input.Tree.getInline("radiance");
    } else {
        input.Stream << ", color_mulf(" << input.Tree.getInline("irradiance");
        if (mUseRadius)
            input.Stream << ",1 / sun_area_from_radius(" << input.Tree.getInline("radius") << "))";
        else
            input.Stream << ",1 / sun_area_from_cos_angle(math_builtins::cos(rad(" << input.Tree.getInline("angle") << "/2))))";
    }
        
    input.Stream << ", false);" << std::endl;

    input.Tree.endClosure();
}

} // namespace IG