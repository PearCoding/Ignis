#include "CheckerBoardPattern.h"
#include "SceneObject.h"
#include "loader/LoaderUtils.h"
#include "loader/ShadingTree.h"

namespace IG {
CheckerBoardPattern::CheckerBoardPattern(const std::string& name, const std::shared_ptr<SceneObject>& object)
    : Pattern(name, "checkerboard")
    , mObject(object)
{
}

void CheckerBoardPattern::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addColor("color0", *mObject, Vector3f::Zero());
    input.Tree.addColor("color1", *mObject, Vector3f::Ones());
    input.Tree.addNumber("scale_x", *mObject, 2.0f);
    input.Tree.addNumber("scale_y", *mObject, 2.0f);

    const Transformf transform = mObject->property("transform").getTransform();

    const std::string tex_id = input.Tree.getClosureID(name());
    input.Stream << input.Tree.pullHeader()
                 << "  let tex_" << tex_id << " : Texture = make_checkerboard_texture("
                 << "make_vec2(" << input.Tree.getInline("scale_x") << ", " << input.Tree.getInline("scale_y") << "), "
                 << input.Tree.getInline("color0") << ", "
                 << input.Tree.getInline("color1") << ", "
                 << LoaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG