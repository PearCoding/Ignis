#include "MeshUtils.h"
#include "mesh/ObjFile.h"
#include "mesh/PlyFile.h"

namespace IG {

bool convert_ply_obj(const Path& input, const Path& output)
{
    const auto mesh = ply::load(input);
    if (mesh.vertices.size() == 0)
        return false;

    return obj::save(mesh, output);
}

bool convert_obj_ply(const Path& input, const Path& output)
{
    const auto mesh = obj::load(input);
    if (mesh.vertices.size() == 0)
        return false;

    return ply::save(mesh, output);
}

} // namespace IG