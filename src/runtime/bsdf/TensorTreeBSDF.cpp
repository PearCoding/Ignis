#include "TensorTreeBSDF.h"
#include "SceneObject.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderUtils.h"
#include "loader/ShadingTree.h"
#include "measured/TensorTreeLoader.h"

namespace IG {
TensorTreeBSDF::TensorTreeBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "tensortree")
    , mBSDF(bsdf)
{
}
using TTExportedData = std::pair<std::string, TensorTreeSpecification>;
static TTExportedData setup_tensortree(const std::string& name, const std::shared_ptr<SceneObject>& bsdf, LoaderContext& ctx)
{
    auto filename = ctx.handlePath(bsdf->property("filename").getString(), *bsdf);

    const std::string exported_id = "_tt_" + filename.u8string();

    const auto data = ctx.Cache->ExportedData.find(exported_id);
    if (data != ctx.Cache->ExportedData.end())
        return std::any_cast<TTExportedData>(data->second);

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/tt_" + LoaderUtils::escapeIdentifier(name) + ".bin";

    TensorTreeSpecification spec{};
    if (!TensorTreeLoader::prepare(filename, path, spec))
        ctx.signalError();

    const TTExportedData res             = { path, spec };
    ctx.Cache->ExportedData[exported_id] = res;
    return res;
}

static inline std::string dump_tt_specification(const TensorTreeSpecification& parent, const TensorTreeComponentSpecification& spec)
{
    std::stringstream stream;
    stream << "TensorTreeComponentSpecification{ ndim=" << parent.ndim
           << ", node_count=" << spec.node_count
           << ", value_count=" << spec.value_count
           << ", total=" << spec.total
           << ", root_is_leaf=" << (spec.root_is_leaf ? "true" : "false")
           << "}";
    return stream.str();
}

static inline std::string dump_tt_specification(const TensorTreeSpecification& spec)
{
    std::stringstream stream;
    stream << "TensorTreeSpecification{ ndim=" << spec.ndim
           << ", front_reflection=" << dump_tt_specification(spec, spec.front_reflection)
           << ", back_reflection=" << dump_tt_specification(spec, spec.back_reflection)
           << ", front_transmission=" << dump_tt_specification(spec, spec.front_transmission)
           << ", back_transmission=" << dump_tt_specification(spec, spec.back_transmission)
           << "}";
    return stream.str();
}

void TensorTreeBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    input.Tree.addColor("base_color", *mBSDF, Vector3f::Ones());

    const Vector3f upVector = mBSDF->property("up").getVector3(Vector3f::UnitZ()).normalized();

    const auto data = setup_tensortree(name(), mBSDF, input.Tree.context());

    const std::string buffer_path      = std::get<0>(data);
    const TensorTreeSpecification spec = std::get<1>(data);

    size_t res_id             = input.Tree.context().registerExternalResource(buffer_path);
    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let tt_" << bsdf_id << " = make_tensortree_model(device.request_debug_output(), device.load_buffer_by_id(" << res_id << "), "
                 << dump_tt_specification(spec) << ");" << std::endl
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_tensortree_bsdf(ctx.surf, "
                 << input.Tree.getInline("base_color") << ", "
                 << LoaderUtils::inlineVector(upVector) << ", "
                 << "tt_" << bsdf_id << ");" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG