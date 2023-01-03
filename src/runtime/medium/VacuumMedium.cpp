#include "VacuumMedium.h"
#include "loader/ShadingTree.h"

namespace IG {
VacuumMedium::VacuumMedium(const std::string& name)
    : Medium(name, "vacuum")
{
}

void VacuumMedium::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    const std::string media_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let medium_" << media_id << " : MediumGenerator = @|ctx| { maybe_unused(ctx); make_vacuum_medium() };" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG