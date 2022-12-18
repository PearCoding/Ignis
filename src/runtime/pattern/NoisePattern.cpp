#include "NoisePattern.h"
#include "SceneObject.h"
#include "loader/LoaderUtils.h"
#include "loader/ShadingTree.h"

namespace IG {
inline static std::string strTypeFunc(NoisePattern::Type type)
{
    switch (type) {
    default:
    case NoisePattern::Type::Noise:
        return "noise";
    case NoisePattern::Type::CellNoise:
        return "cellnoise";
    case NoisePattern::Type::PNoise:
        return "pnoise";
    case NoisePattern::Type::Perlin:
        return "perlin";
    case NoisePattern::Type::Voronoi:
        return "voronoi";
    case NoisePattern::Type::FBM:
        return "fbm";
    }
}

NoisePattern::NoisePattern(Type type, const std::string& name, const std::shared_ptr<SceneObject>& object)
    : Pattern(name, strTypeFunc(type))
    , mObject(object)
    , mType(type)
{
}

void NoisePattern::serialize(const SerializationInput& input) const
{
    constexpr float DefaultSeed = 36326639.0f;

    input.Tree.beginClosure(name());

    input.Tree.addColor("color", *mObject, Vector3f::Ones());
    input.Tree.addNumber("seed", *mObject, DefaultSeed);
    input.Tree.addNumber("scale_x", *mObject, 10.0f);
    input.Tree.addNumber("scale_y", *mObject, 10.0f);

    const Transformf transform = mObject->property("transform").getTransform();
    const std::string func     = strTypeFunc(mType);
    const std::string afunc    = mObject->property("colored").getBool() ? "c" + func : func;

    const std::string tex_id = input.Tree.getClosureID(name());
    input.Stream << input.Tree.pullHeader()
                 << "  let tex_" << tex_id << " : Texture = make_" << afunc << "_texture("
                 << "make_vec2(" << input.Tree.getInline("scale_x") << ", " << input.Tree.getInline("scale_y") << "), "
                 << input.Tree.getInline("color") << ", "
                 << input.Tree.getInline("seed") << ", "
                 << LoaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG