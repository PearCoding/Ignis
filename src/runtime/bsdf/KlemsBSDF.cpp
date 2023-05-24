#include "KlemsBSDF.h"
#include "SceneObject.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderUtils.h"
#include "loader/ShadingTree.h"
#include "measured/KlemsLoader.h"

namespace IG {
KlemsBSDF::KlemsBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "klems")
    , mBSDF(bsdf)
{
}

using KlemsExportedData = std::pair<Path, KlemsSpecification>;
static KlemsExportedData setup_klems(const std::string& name, const std::shared_ptr<SceneObject>& bsdf, LoaderContext& ctx)
{
    auto filename = ctx.handlePath(bsdf->property("filename").getString(), *bsdf);

    const std::string exported_id = "_klems_" + filename.u8string();

    const auto data = ctx.Cache->ExportedData.find(exported_id);
    if (data != ctx.Cache->ExportedData.end())
        return std::any_cast<KlemsExportedData>(data->second);

    const Path path = ctx.CacheManager->directory() / ("klems_" + LoaderUtils::escapeIdentifier(name) + ".bin");

    KlemsSpecification spec{};
    if (!KlemsLoader::prepare(filename, path, spec))
        ctx.signalError();

    const KlemsExportedData res          = { path, spec };
    ctx.Cache->ExportedData[exported_id] = res;
    return res;
}

static inline std::string dump_klems_specification(const KlemsComponentSpecification& spec)
{
    std::stringstream stream;
    stream << "KlemsComponentSpecification{ total=" << spec.total
           << ", theta_count=[" << spec.theta_count.first << ", " << spec.theta_count.second << "]"
           << ", entry_count=[" << spec.entry_count.first << ", " << spec.entry_count.second << "]"
           << "}";
    return stream.str();
}

static inline std::string dump_klems_specification(const KlemsSpecification& spec)
{
    std::stringstream stream;
    stream << "KlemsSpecification{"
           << "  front_reflection=" << dump_klems_specification(spec.front_reflection)
           << ", back_reflection=" << dump_klems_specification(spec.back_reflection)
           << ", front_transmission=" << dump_klems_specification(spec.front_transmission)
           << ", back_transmission=" << dump_klems_specification(spec.back_transmission)
           << "}";
    return stream.str();
}

void KlemsBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    input.Tree.addColor("base_color", *mBSDF, Vector3f::Ones());

    const Vector3f upVector = mBSDF->property("up").getVector3(Vector3f::UnitZ()).normalized();

    const auto data = setup_klems(name(), mBSDF, input.Tree.context());

    const Path buffer_path = std::get<0>(data);
    const KlemsSpecification spec           = std::get<1>(data);

    size_t res_id             = input.Tree.context().registerExternalResource(buffer_path);
    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let klems_" << bsdf_id << " = make_klems_model(device.load_buffer_by_id(" << res_id << "), "
                 << dump_klems_specification(spec) << ");" << std::endl
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_klems_bsdf(ctx.surf, "
                 << input.Tree.getInline("base_color") << ", "
                 << LoaderUtils::inlineVector(upVector) << ", "
                 << "klems_" << bsdf_id << ");" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG