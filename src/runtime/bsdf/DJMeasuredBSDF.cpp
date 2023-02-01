#include "DJMeasuredBSDF.h"
#include "SceneObject.h"
#include "loader/LoaderContext.h"
#include "loader/ShadingTree.h"
#include "measured/djmeasured.h"

namespace IG {
DJMeasuredBSDF::DJMeasuredBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "djmeasured")
    , mBSDF(bsdf)
{
}

void DJMeasuredBSDF::serialize(const SerializationInput& input) const
{
    // full file path to measured BRDF is given
    std::string full_path = mBSDF->property("filename").getString();

    // find last path separator
    auto last_sep = full_path.find_last_of('/');
    // extract bsdf name by also getting rid of file extension (.bsdf)
    auto bsdf_name          = full_path.substr(last_sep + 1, full_path.length() - last_sep - 6);
    auto filename           = input.Tree.context().handlePath(full_path, *mBSDF);
    std::string buffer_name = "buffer_" + bsdf_name;

    // saving of converted brdf data in /data directory (taken from klems loader)
    std::filesystem::path out_path = input.Tree.context().CacheManager->directory() / ("djmeasured_" + bsdf_name);

    measured::BRDFData* data = measured::load_brdf_data(filename);
    measured::write_brdf_data(data, out_path);

    input.Tree.beginClosure(name());
    input.Tree.addColor("tint", *mBSDF, Vector3f::Ones());

    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let " << buffer_name << "_ndf : DeviceBuffer = device.load_buffer(\"" << out_path << "_ndf\");\n"
                 << "  let " << buffer_name << "_vndf : DeviceBuffer = device.load_buffer(\"" << out_path << "_vndf\");\n"
                 << "  let " << buffer_name << "_sigma : DeviceBuffer = device.load_buffer(\"" << out_path << "_sigma\");\n"
                 << "  let " << buffer_name << "_luminance : DeviceBuffer = device.load_buffer(\"" << out_path << "_luminance\");\n"
                 << "  let " << buffer_name << "_rgb : DeviceBuffer = device.load_buffer(\"" << out_path << "_rgb\");\n"
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_djmeasured_bsdf(ctx.surf, "
                 << (data->isotropic ? "true" : "false") << ", "
                 << (data->jacobian ? "true" : "false") << ", "
                 << buffer_name << "_ndf, "
                 << buffer_name << "_vndf, "
                 << buffer_name << "_sigma, "
                 << buffer_name << "_luminance, "
                 << buffer_name << "_rgb, "
                 << input.Tree.getInline("tint")
                 << ");" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG