#include "EnvironmentLight.h"
#include "Logger.h"
#include "loader/LoaderTexture.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
EnvironmentLight::EnvironmentLight(const std::string& name, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mUseCDF          = light->property("cdf").getBool(true);
    mUseCompensation = light->property("compensate").getBool(true);
}

float EnvironmentLight::computeFlux(const ShadingTree& tree) const
{
    const float radius   = tree.context().SceneDiameter / 2;
    const float radiance = tree.computeNumber("radiance", *mLight, 1.0f);
    const float scale    = tree.computeNumber("scale", *mLight, 1.0f);
    return scale * radiance * Pi * radius * radius;
}

void EnvironmentLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    ShadingTree::BakeOutputTexture baked;
    const std::string exported_id = "_light_" + name();
    const auto cache_data         = input.Tree.context().ExportedData.find(exported_id);
    if (cache_data != input.Tree.context().ExportedData.end()) {
        baked = std::any_cast<ShadingTree::BakeOutputTexture>(cache_data->second);
    } else {
        baked = input.Tree.bakeTexture("radiance", *mLight, Vector3f::Ones(), ShadingTree::TextureBakeOptions{ 1024, 256, true });

        input.Tree.context().ExportedData[exported_id] = baked;
    }

    input.Tree.addColor("scale", *mLight, Vector3f::Ones());
    input.Tree.addTexture("radiance", *mLight, Vector3f::Ones());
    const Matrix3f trans = mLight->property("transform").getTransform().linear().transpose().inverse();

    const std::string light_id = input.Tree.currentClosureID();
    if (mUseCDF && baked.has_value() && baked.value()->width > 1 && baked.value()->height > 1) {
        const auto cdf          = LoaderUtils::setup_cdf2d(input.Tree.context(), light_id, *baked.value(), true, mUseCompensation);
        const size_t res_cdf_id = input.Tree.context().registerExternalResource(std::get<0>(cdf));
        input.Stream << input.Tree.pullHeader()
                     << "  let cdf_" << light_id << "   = cdf::make_cdf_2d_from_buffer(device.load_buffer_by_id(" << res_cdf_id << "), " << std::get<1>(cdf) << ", " << std::get<2>(cdf) << ");" << std::endl
                     << "  let light_" << light_id << " = make_environment_light_textured(" << input.ID
                     << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
                     << ", " << input.Tree.getInline("scale")
                     << ", " << input.Tree.getInline("radiance")
                     << ", cdf_" << light_id
                     << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;
    } else {
        input.Stream << input.Tree.pullHeader()
                     << "  let light_" << light_id << " = make_environment_light(" << input.ID
                     << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
                     << ", " << input.Tree.getInline("scale")
                     << ", " << input.Tree.getInline("radiance")
                     << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;
    }

    input.Tree.endClosure();
}

} // namespace IG