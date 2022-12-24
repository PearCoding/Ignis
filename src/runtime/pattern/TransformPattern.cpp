#include "TransformPattern.h"
#include "SceneObject.h"
#include "loader/LoaderUtils.h"
#include "loader/ShadingTree.h"

namespace IG {
TransformPattern::TransformPattern(const std::string& name, const std::shared_ptr<SceneObject>& object)
    : Pattern(name, "transform")
    , mObject(object)
{
}

void TransformPattern::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addTexture("texture", *mObject, Vector3f::Zero());

    const Transformf transform = mObject->property("transform").getTransform();

    const std::string tex_id = input.Tree.getClosureID(name());
    input.Stream << input.Tree.pullHeader()
                 << "  let tex_" << tex_id << " : Texture = make_transform_texture("
                 << input.Tree.getInline("texture") << ", "
                 << LoaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG