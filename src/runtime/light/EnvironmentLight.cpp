#include "EnvironmentLight.h"
#include "Logger.h"
#include "loader/LoaderTexture.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
EnvironmentLight::EnvironmentLight(const std::string& name, const std::shared_ptr<Parser::Object>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mUseCDF          = light->property("cdf").getBool(true);
    mUseCompensation = light->property("compensate").getBool(true);
}

float EnvironmentLight::computeFlux(const ShadingTree& tree) const
{
    const float radius   = tree.context().Environment.SceneDiameter / 2;
    const float radiance = tree.computeNumber("radiance", *mLight, 1.0f);
    const float scale    = tree.computeNumber("scale", *mLight, 1.0f);
    return scale * radiance * Pi * radius * radius;
}

void EnvironmentLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addColor("scale", *mLight, Vector3f::Ones(), true);
    input.Tree.addTexture("radiance", *mLight, true);
    const Matrix3f trans = mLight->property("transform").getTransform().linear().transpose().inverse();

    const std::string light_id = input.Tree.currentClosureID();
    if (input.Tree.isPureTexture("radiance")) {
        const std::string tex_name = mLight->property("radiance").getString();
        const auto tex             = input.Tree.context().Scene.texture(tex_name);
        if (!tex) {
            IG_LOG(L_ERROR) << "Unknown texture '" << tex_name << "'" << std::endl;
            input.Tree.signalError();
            return; // TODO
        }

        const std::string tex_path = LoaderTexture::getFilename(*tex, input.Tree.context()).generic_u8string();
        if (!mUseCDF || tex_path.empty()) {
            input.Stream << input.Tree.pullHeader()
                         << "  let light_" << light_id << " = make_environment_light(" << input.ID
                         << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
                         << ", " << input.Tree.getInline("scale")
                         << ", tex_" << input.Tree.getClosureID(tex_name)
                         << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;
        } else {
            IG_LOG(L_DEBUG) << "Using environment cdf for '" << tex_name << "'" << std::endl;
            const auto cdf          = LoaderUtils::setup_cdf2d(input.Tree.context(), tex_path, true, mUseCompensation);
            const size_t res_cdf_id = input.Tree.context().registerExternalResource(std::get<0>(cdf));
            input.Stream << input.Tree.pullHeader()
                         << "  let cdf_" << light_id << "   = cdf::make_cdf_2d_from_buffer(device.load_buffer_by_id(" << res_cdf_id << "), " << std::get<1>(cdf) << ", " << std::get<2>(cdf) << ");" << std::endl
                         << "  let light_" << light_id << " = make_environment_light_textured(" << input.ID
                         << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
                         << ", " << input.Tree.getInline("scale")
                         << ", tex_" << input.Tree.getClosureID(tex_name)
                         << ", cdf_" << light_id
                         << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;
        }
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