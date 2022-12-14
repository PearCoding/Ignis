#include "SkyLight.h"
#include "Logger.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "skysun/SkyModel.h"

namespace IG {
SkyLight::SkyLight(const std::string& name, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    auto ea       = LoaderUtils::getEA(*light);
    mSunDirection = ea.toDirectionYUp();

    auto ground    = light->property("ground").getVector3(Vector3f(0.8f, 0.8f, 0.8f));
    auto turbidity = light->property("turbidity").getNumber(3.0f);

    SkyModel model(RGB(ground), ea, turbidity);
    mTotalFlux = model.computeTotal().average();
}

float SkyLight::computeFlux(const ShadingTree& tree) const
{
    const float radius = tree.context().SceneDiameter / 2;
    const float scale  = tree.computeNumber("scale", *mLight, 1.0f);
    return mTotalFlux * scale * radius * radius;
}

static std::string setup_sky(LoaderContext& ctx, const std::string& name, const std::shared_ptr<SceneObject>& light)
{
    const std::string exported_id = "_sky_" + name;

    const auto data = ctx.ExportedData.find(exported_id);
    if (data != ctx.ExportedData.end())
        return std::any_cast<std::string>(data->second);

    auto ground    = light->property("ground").getVector3(Vector3f(0.8f, 0.8f, 0.8f));
    auto turbidity = light->property("turbidity").getNumber(3.0f);
    const auto ea  = LoaderUtils::getEA(*light);

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/skytex_" + LoaderUtils::escapeIdentifier(name) + ".exr";
    SkyModel model(RGB(ground), ea, turbidity);
    model.save(path);

    ctx.ExportedData[exported_id] = path;
    return path;
}

void SkyLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addColor("scale", *mLight, Vector3f::Ones(), true);

    const std::string path = setup_sky(input.Tree.context(), name(), mLight);
    const auto cdf         = LoaderUtils::setup_cdf2d(input.Tree.context(), path, true, false);

    const Matrix3f trans = mLight->property("transform").getTransform().linear().transpose().inverse();

    size_t res_img_id          = input.Tree.context().registerExternalResource(path);
    size_t res_cdf_id          = input.Tree.context().registerExternalResource(std::get<0>(cdf));
    const std::string light_id = input.Tree.currentClosureID();

    input.Stream << input.Tree.pullHeader()
                 << "  let sky_tex_" << light_id << " = make_image_texture(make_repeat_border(), make_bilinear_filter(), device.load_image_by_id(" << res_img_id << ", 4), mat3x3_identity());" << std::endl // TODO: Refactor this out
                 << "  let sky_cdf_" << light_id << " = cdf::make_cdf_2d_from_buffer(device.load_buffer_by_id(" << res_cdf_id << "), " << std::get<1>(cdf) << ", " << std::get<2>(cdf) << ");" << std::endl
                 << "  let light_" << light_id << "   = make_environment_light_textured(" << input.ID
                 << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
                 << ", " << input.Tree.getInline("scale")
                 << ", sky_tex_" << light_id << ", sky_cdf_" << light_id
                 << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;

    input.Tree.endClosure();
}

} // namespace IG