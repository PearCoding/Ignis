#include "PerezLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
PerezLight::PerezLight(const std::string& name, const std::shared_ptr<Parser::Object>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mSunDirection = LoaderUtils::getDirection(*light);
}

float PerezLight::computeFlux(const ShadingTree& tree) const
{
    // TODO
    const float radius = tree.context().Environment.SceneDiameter / 2;
    return Pi * radius * radius;
}

void PerezLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addNumber("a", *mLight, 1.0f, true);
    input.Tree.addNumber("b", *mLight, 1.0f, true);
    input.Tree.addNumber("c", *mLight, 1.0f, true);
    input.Tree.addNumber("d", *mLight, 1.0f, true);
    input.Tree.addNumber("e", *mLight, 1.0f, true);

    const Matrix3f trans = mLight->property("transform").getTransform().linear().transpose().inverse();

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
                     << ", calc_perez(" << std::min(1.0f, std::max(-1.0f, -mSunDirection(1)))
                     << ", 1, " << input.Tree.getInline("a")
                     << ", " << input.Tree.getInline("b")
                     << ", " << input.Tree.getInline("c")
                     << ", " << input.Tree.getInline("d")
                     << ", " << input.Tree.getInline("e")
                     << "))";
    }

    input.Stream << ", " << input.Tree.getInline("a")
                 << ", " << input.Tree.getInline("b")
                 << ", " << input.Tree.getInline("c")
                 << ", " << input.Tree.getInline("d")
                 << ", " << input.Tree.getInline("e")
                 << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;

    input.Tree.endClosure();
}

} // namespace IG