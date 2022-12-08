#include "Medium.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
void Medium::handleReferenceEntity(const Parser::Object& obj)
{
    mReferenceEntity = obj.property("reference").getString();
}

std::string Medium::generateReferencePMS(const SerializationInput& input) const
{
    const std::string media_id = input.Tree.currentClosureID();
    if (!isReferenceEntityIDSet())
        return {};

    input.Stream << "  let pms_" << media_id << " = @|| {" << std::endl
                 << "    let entity = entities(" << mReferenceEntityID << ");" << std::endl
                 << "    make_standard_pointmapperset(shapes(entity.shape_id), entity)" << std::endl
                 << "  };" << std::endl;

    return "pms_" + media_id;
}
} // namespace IG