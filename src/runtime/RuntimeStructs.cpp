#include "RuntimeStructs.h"
#include <sstream>

namespace IG {
std::string ParameterSet::dump() const
{
    std::stringstream stream;
    for (auto p : IntParameters)
        stream << "[i32] " << p.first << " = " << p.second << std::endl;
    for (auto p : FloatParameters)
        stream << "[f32] " << p.first << " = " << p.second << std::endl;
    for (auto p : VectorParameters)
        stream << "[vec3] " << p.first << " = [" << p.second.x() << ", " << p.second.y() << "," << p.second.z() << "]" << std::endl;
    for (auto p : ColorParameters)
        stream << "[color] " << p.first << " = [" << p.second.x() << ", " << p.second.y() << "," << p.second.z() << "," << p.second.w() << "]" << std::endl;

    return stream.str();
}

void ParameterSet::mergeFrom(const ParameterSet& other)
{
    for (auto p : other.IntParameters)
        IntParameters.try_emplace(p.first, p.second);
    for (auto p : other.FloatParameters)
        FloatParameters.try_emplace(p.first, p.second);
    for (auto p : other.VectorParameters)
        VectorParameters.try_emplace(p.first, p.second);
    for (auto p : other.ColorParameters)
        ColorParameters.try_emplace(p.first, p.second);
}

} // namespace IG