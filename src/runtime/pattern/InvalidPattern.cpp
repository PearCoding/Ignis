#include "InvalidPattern.h"
#include "loader/ShadingTree.h"

namespace IG {
InvalidPattern::InvalidPattern(const std::string& name)
    : Pattern(name, "invalid")
{
}

void InvalidPattern::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    const std::string tex_id = input.Tree.getClosureID(name());
    input.Stream << inlineError(tex_id) << std::endl;
    input.Tree.endClosure();
}

std::string InvalidPattern::inlineError(const std::string& name)
{
    return "  let tex_" + name + " : Texture = make_invalid_texture();";
}
} // namespace IG