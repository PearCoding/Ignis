#include "SunLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
SunLight::SunLight(const std::string& name, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mDirection = LoaderUtils::getDirection(*light);
}

float SunLight::computeFlux(ShadingTree& tree) const
{
    IG_UNUSED(tree);

    const float sun_scale        = mLight->property("sun_scale").getNumber(1);
    const float sun_radius_scale = mLight->property("sun_radius_scale").getNumber(1);
    return sun_scale * sun_radius_scale * Pi;
}

void SunLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addNumber("sun_scale", *mLight, 1);
    input.Tree.addNumber("sun_radius_scale", *mLight, 1);

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let light_" << light_id << " = make_sun_light(" << input.ID
                 << ", " << LoaderUtils::inlineVector(mDirection)
                 << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
                 << ", " << input.Tree.getInline("sun_radius_scale")
                 << ", color_mulf(color_builtins::white, " << input.Tree.getInline("sun_scale") << "));" << std::endl;

    input.Tree.endClosure();
}

} // namespace IG