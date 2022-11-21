#include "PerezLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "skysun/PerezModel.h"

namespace IG {
PerezLight::PerezLight(const std::string& name, const std::shared_ptr<Parser::Object>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mSunDirection = LoaderUtils::getDirection(*light);
    mTimePoint    = LoaderUtils::getTimePoint(*light);
    mHasGround    = mLight->property("has_ground").getBool(true);
    std::cout << mSunDirection << std::endl;
}

float PerezLight::computeFlux(const ShadingTree& tree) const
{
    // TODO
    const float radius = tree.context().Environment.SceneDiameter / 2;
    return Pi * radius * radius;
}

void PerezLight::serialize(const SerializationInput& input) const
{
    const float sin_elevation = std::min(1.0f, std::max(-1.0f, -mSunDirection(1)));
    const float solar_zenith  = std::acos(std::min(1.0f, std::max(-1.0f, mSunDirection(1))));

    input.Tree.beginClosure(name());
    input.Tree.addColor("ground", *mLight, Vector3f::Ones(), true);

    const Matrix3f trans = mLight->property("transform").getTransform().linear().transpose().inverse();

    const auto insertModel = [&](const PerezModel& model) {
        // Will replace potential other parameters
        input.Tree.insertNumber("a", Parser::Property::fromNumber(model.a()));
        input.Tree.insertNumber("b", Parser::Property::fromNumber(model.b()));
        input.Tree.insertNumber("c", Parser::Property::fromNumber(model.c()));
        input.Tree.insertNumber("d", Parser::Property::fromNumber(model.d()));
        input.Tree.insertNumber("e", Parser::Property::fromNumber(model.e()));
    };

    // Other input specifications
    if (mLight->properties().count("clearness") || mLight->properties().count("brightness")) {
        insertModel(PerezModel::fromSky(mLight->property("brightness").getNumber(0.2f),
                                        mLight->property("clearness").getNumber(1),
                                        solar_zenith));
    } else if (mLight->properties().count("direct_irradiance") || mLight->properties().count("diffuse_irradiance")) {
        insertModel(PerezModel::fromIrrad(mLight->property("diffuse_irradiance").getNumber(1),
                                          mLight->property("direct_irradiance").getNumber(1),
                                          solar_zenith,
                                          mTimePoint.dayOfTheYear()));
    } else if (mLight->properties().count("direct_illuminance") || mLight->properties().count("diffuse_illuminance")) {
        insertModel(PerezModel::fromIllum(mLight->property("diffuse_illuminance").getNumber(1),
                                          mLight->property("direct_illuminance").getNumber(1),
                                          solar_zenith,
                                          mTimePoint.dayOfTheYear()));
    } else {
        input.Tree.addNumber("a", *mLight, 1.0f, true);
        input.Tree.addNumber("b", *mLight, 1.0f, true);
        input.Tree.addNumber("c", *mLight, 1.0f, true);
        input.Tree.addNumber("d", *mLight, 1.0f, true);
        input.Tree.addNumber("e", *mLight, 1.0f, true);
    }

    bool usesLuminance = false;
    if (mLight->properties().count("luminance")) {
        input.Tree.addColor("luminance", *mLight, Vector3f::Ones(), true);
        usesLuminance = true;
    } else {
        input.Tree.addColor("zenith", *mLight, Vector3f::Ones(), true);
        usesLuminance = false;
    }

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let light_" << light_id << " = make_perez_light(" << input.ID
                 << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
                 << ", " << LoaderUtils::inlineVector(mSunDirection);

    if (usesLuminance) {
        input.Stream << ", " << input.Tree.getInline("luminance");
    } else {
        input.Stream << ", color_mulf(" << input.Tree.getInline("zenith")
                     << ", calc_perez(" << sin_elevation
                     << ", 1, " << input.Tree.getInline("a")
                     << ", " << input.Tree.getInline("b")
                     << ", " << input.Tree.getInline("c")
                     << ", " << input.Tree.getInline("d")
                     << ", " << input.Tree.getInline("e")
                     << "))";
    }

    input.Stream << ", " << input.Tree.getInline("ground")
                 << ", " << input.Tree.getInline("a")
                 << ", " << input.Tree.getInline("b")
                 << ", " << input.Tree.getInline("c")
                 << ", " << input.Tree.getInline("d")
                 << ", " << input.Tree.getInline("e")
                 << ", " << (mHasGround ? "true" : "false")
                 << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;

    input.Tree.endClosure();
}

} // namespace IG