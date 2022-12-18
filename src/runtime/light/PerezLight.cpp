#include "PerezLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "skysun/Illuminance.h"
#include "skysun/PerezModel.h"

namespace IG {
PerezLight::PerezLight(const std::string& name, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mSunDirection = LoaderUtils::getDirection(*light);
    mTimePoint    = LoaderUtils::getTimePoint(*light);
    mHasGround    = mLight->property("has_ground").getBool(true);
}

float PerezLight::computeFlux(ShadingTree& tree) const
{
    // TODO
    const float radius = tree.context().SceneDiameter / 2;
    return Pi * radius * radius;
}

static std::tuple<PerezModel, float> getModel(const SceneObject& obj, float solar_zenith, const TimePoint& timepoint)
{
    if (obj.properties().count("clearness") || obj.properties().count("brightness")) {
        const PerezModel model = PerezModel::fromSky(obj.property("brightness").getNumber(0.2f),
                                                     obj.property("clearness").getNumber(1),
                                                     solar_zenith);
        return { model,
                 PerezModel::computeDiffuseIrradiance(obj.property("brightness").getNumber(0.2f), solar_zenith, timepoint.dayOfTheYear()) };
    } else if (obj.properties().count("direct_irradiance") || obj.properties().count("diffuse_irradiance")) {
        const PerezModel model = PerezModel::fromIrrad(obj.property("diffuse_irradiance").getNumber(1),
                                                       obj.property("direct_irradiance").getNumber(1),
                                                       solar_zenith,
                                                       timepoint.dayOfTheYear());
        return { model,
                 obj.property("diffuse_irradiance").getNumber(1) };
    } else if (obj.properties().count("direct_illuminance") || obj.properties().count("diffuse_illuminance")) {
        const PerezModel model = PerezModel::fromIllum(obj.property("diffuse_illuminance").getNumber(1),
                                                       obj.property("direct_illuminance").getNumber(1),
                                                       solar_zenith,
                                                       timepoint.dayOfTheYear());
        return { model,
                 convertIlluminanceToIrradiance(obj.property("diffuse_illuminance").getNumber(1)) /* TODO: Not that simple */ };
    } else {
        return { PerezModel::fromParameters(
                     obj.property("a").getNumber(1),
                     obj.property("b").getNumber(1),
                     obj.property("c").getNumber(1),
                     obj.property("d").getNumber(1),
                     obj.property("e").getNumber(1)),
                 1.0f /* TODO*/ };
    }
}

void PerezLight::serialize(const SerializationInput& input) const
{
    // TODO: No support for PExpr

    const float sin_elevation = std::min(1.0f, std::max(-1.0f, -mSunDirection(1)));
    const float solar_zenith  = std::acos(std::min(1.0f, std::max(-1.0f, mSunDirection(1))));

    input.Tree.beginClosure(name());
    input.Tree.addColor("ground", *mLight, Vector3f::Ones());

    const Matrix3f trans = mLight->property("transform").getTransform().linear().transpose().inverse();

    // Other input specifications
    const auto [model, diff_irrad] = getModel(*mLight, solar_zenith, mTimePoint);
    const float diffnorm           = diff_irrad / model.integrate(solar_zenith);

    bool usesLuminance = false;
    if (mLight->properties().count("luminance")) {
        input.Tree.addColor("luminance", *mLight, Vector3f::Ones());
        usesLuminance = true;
    } else {
        input.Tree.addColor("zenith", *mLight, Vector3f::Ones());
        usesLuminance = false;
    }

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let light_" << light_id << " = make_perez_light(" << input.ID
                 << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
                 << ", " << LoaderUtils::inlineVector(mSunDirection);

    if (usesLuminance) {
        input.Stream << ", color_mulf(" << input.Tree.getInline("luminance")
                     << ", " << diffnorm << ")";
    } else {
        input.Stream << ", color_mulf(" << input.Tree.getInline("zenith")
                     << ", calc_perez(" << sin_elevation
                     << ", 1, " << model.a()
                     << ", " << model.b()
                     << ", " << model.c()
                     << ", " << model.d()
                     << ", " << model.e()
                     << ") * " << diffnorm << ")";
    }

    input.Stream << ", " << input.Tree.getInline("ground")
                 << ", " << model.a()
                 << ", " << model.b()
                 << ", " << model.c()
                 << ", " << model.d()
                 << ", " << model.e()
                 << ", " << (mHasGround ? "true" : "false")
                 << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;

    input.Tree.endClosure();
}

} // namespace IG