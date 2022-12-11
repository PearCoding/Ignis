#include "DirectionalLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
DirectionalLight::DirectionalLight(const std::string& name, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mDirection = LoaderUtils::getDirection(*light);
}

float DirectionalLight::computeFlux(const ShadingTree& tree) const
{
    const float radius = tree.context().SceneDiameter / 2;
    return tree.computeNumber("irradiance", *mLight, 1) * Pi * radius * radius;
}

void DirectionalLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addColor("irradiance", *mLight, Vector3f::Ones(), true);

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let light_" << light_id << " = make_directional_light(" << input.ID
                 << ", " << LoaderUtils::inlineVector(mDirection)
                 << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
                 << ", " << input.Tree.getInline("irradiance") << ");" << std::endl;

    input.Tree.endClosure();
}

} // namespace IG