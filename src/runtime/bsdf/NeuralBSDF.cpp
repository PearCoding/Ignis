#include "NeuralBSDF.h"
#include "SceneObject.h"
#include "loader/LoaderContext.h"
#include "loader/ShadingTree.h"

namespace IG {
NeuralBSDF::NeuralBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "neural")
    , mBSDF(bsdf)
{
}

void NeuralBSDF::serialize(const SerializationInput& input) const
{
    // full file path to measured BRDF is given
    std::string full_path = mBSDF->property("filename").getString();

    // find last path separator
    auto last_sep = full_path.find_last_of('/');
    // extract bsdf name by also getting rid of file extension (.bin)
    auto bsdf_name          = full_path.substr(last_sep + 1, full_path.length() - last_sep - 5);
    auto filename           = input.Tree.context().handlePath(full_path, *mBSDF);
    std::string buffer_name = "buffer_" + bsdf_name;

    input.Tree.beginClosure(name());

    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let " << buffer_name << " : DeviceBuffer = device.load_buffer(" << filename << ");\n"
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| "
                 << "make_neural_bsdf(ctx.surf, " << buffer_name
                 << ");" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG