#include "PerezLight.h"
#include "Logger.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "skysun/Illuminance.h"

namespace IG {
PerezLight::PerezLight(const std::string& name, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mSunDirection = LoaderUtils::getDirection(*light); // Constant version. Can be adaptive in the calculation though
    mTimePoint    = LoaderUtils::getTimePoint(*light);
    mHasGround    = mLight->property("has_ground").getBool(true);
    mHasSun       = mLight->property("has_sun").getBool(true);
}

float PerezLight::computeFlux(ShadingTree& tree) const
{
    // TODO
    const float radius = tree.context().SceneDiameter / 2;
    return Pi * radius * radius;
}

void PerezLight::serialize(const SerializationInput& input) const
{
    enum class CallType {
        BrightnessClearness,
        Irradiance,
        IrradianceHori
        // Illuminance // TODO
    };

    // TODO: No support for PExpr for Time!

    input.Tree.beginClosure(name());
    input.Tree.addColor("ground", *mLight, Vector3f::Ones() * 0.2f);

    if (mLight->hasProperty("up"))
        input.Tree.addVector("up", *mLight, Vector3f::UnitY());
    else
        input.Tree.addComputedMatrix3("_transform", mLight->property("transform").getTransform().linear().transpose().inverse());

    input.Tree.addVector("direction", *mLight, mSunDirection);
    input.Tree.addNumber("day_of_the_year", *mLight, mTimePoint.dayOfTheYear());
    input.Tree.addColor("color", *mLight, Vector3f::Ones()); // Tint color

    CallType callType = CallType::BrightnessClearness;
    if (mLight->hasProperty("direct_irradiance") || mLight->hasProperty("diffuse_irradiance")) {
        if (mLight->hasProperty("direct_horizontal_irradiance") && !mLight->hasProperty("direct_irradiance"))
            callType = CallType::IrradianceHori;
        else
            callType = CallType::Irradiance;
    }

    switch (callType) {
    default:
        input.Tree.addNumber("brightness", *mLight, 0.2f);
        input.Tree.addNumber("clearness", *mLight, 1.0f);
        break;
    case CallType::Irradiance:
        input.Tree.addNumber("diffuse_irradiance", *mLight, 1.0f);
        input.Tree.addNumber("direct_irradiance", *mLight, 1.0f);
        break;
    case CallType::IrradianceHori:
        input.Tree.addNumber("diffuse_irradiance", *mLight, 1.0f);
        input.Tree.addNumber("direct_horizontal_irradiance", *mLight, 1.0f);
        break;
    }

    const char* method_name = "make_perez_light_from_brightness_clearness";
    if (callType == CallType::Irradiance)
        method_name = "make_perez_light_from_irradiance";
    else if (callType == CallType::IrradianceHori)
        method_name = "make_perez_light_from_irradiance_hori";

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let light_" << light_id << " = " << method_name << "(" << input.ID
                 << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
                 << ", vec3_normalize(" << input.Tree.getInline("direction") << ")"
                 << ", " << input.Tree.getInline("color")
                 << ", " << input.Tree.getInline("ground");

    switch (callType) {
    default:
        input.Stream << ", " << input.Tree.getInline("brightness")
                     << ", " << input.Tree.getInline("clearness");
        break;
    case CallType::Irradiance:
        input.Stream << ", " << input.Tree.getInline("diffuse_irradiance")
                     << ", " << input.Tree.getInline("direct_irradiance");
        break;
    case CallType::IrradianceHori:
        input.Stream << ", " << input.Tree.getInline("diffuse_irradiance")
                     << ", " << input.Tree.getInline("direct_horizontal_irradiance");
        break;
    }

    input.Stream << ", " << input.Tree.getInline("day_of_the_year")
                 << ", " << (mHasGround ? "true" : "false")
                 << ", " << (mHasSun ? "true" : "false");

    if (mLight->hasProperty("up")) {
        input.Stream << ", make_cie_sky_transform(vec3_normalize(" << input.Tree.getInline("up") << "))";
    } else {
        input.Stream << ", " << input.Tree.getInlineMatrix3("_transform");
    }

    input.Stream << ");" << std::endl;

    input.Tree.endClosure();
}

} // namespace IG