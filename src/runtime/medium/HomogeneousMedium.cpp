#include "HomogeneousMedium.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
HomogeneousMedium::HomogeneousMedium(const std::string& name, const std::shared_ptr<SceneObject>& medium)
    : Medium(name, "homogeneous")
    , mMedium(medium)
{
    handleReferenceEntity(*medium);
}

void HomogeneousMedium::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addColor("sigma_a", *mMedium, Vector3f::Zero());
    input.Tree.addColor("sigma_s", *mMedium, Vector3f::Zero());
    input.Tree.addNumber("g", *mMedium, 0);

    // const std::string pms_func = generateReferencePMS(input);

    const std::string media_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let medium_" << media_id << " : MediumGenerator = @|ctx| { maybe_unused(ctx); make_homogeneous_medium(" << input.Tree.getInline("sigma_a")
                 << ", " << input.Tree.getInline("sigma_s")
                 << ", make_henyeygreenstein_phase(" << input.Tree.getInline("g") << ")) };" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG