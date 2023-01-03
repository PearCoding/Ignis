#include "BrickPattern.h"
#include "SceneObject.h"
#include "loader/LoaderUtils.h"
#include "loader/ShadingTree.h"

namespace IG {
BrickPattern::BrickPattern(const std::string& name, const std::shared_ptr<SceneObject>& object)
    : Pattern(name, "brick")
    , mObject(object)
{
}

void BrickPattern::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addColor("color0", *mObject, Vector3f::Zero());
    input.Tree.addColor("color1", *mObject, Vector3f::Ones());
    input.Tree.addNumber("scale_x", *mObject, 3.0f);
    input.Tree.addNumber("scale_y", *mObject, 6.0f);
    input.Tree.addNumber("gap_x", *mObject, 0.05f);
    input.Tree.addNumber("gap_y", *mObject, 0.1f);

    const Transformf transform = mObject->property("transform").getTransform();

    const std::string tex_id = input.Tree.getClosureID(name());
    input.Stream << input.Tree.pullHeader()
                 << "  let tex_" << tex_id << " : Texture = make_brick_texture("
                 << input.Tree.getInline("color0") << ", "
                 << input.Tree.getInline("color1") << ", "
                 << "make_vec2(" << input.Tree.getInline("scale_x") << ", " << input.Tree.getInline("scale_y") << "), "
                 << "make_vec2(" << input.Tree.getInline("gap_x") << ", " << input.Tree.getInline("gap_y") << "), "
                 << LoaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG