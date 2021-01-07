#include "IO.h"
#include "Buffer.h"
#include "mesh/TriMesh.h"

namespace IG {
namespace IO {
void write_tri_mesh(const std::string& suffix, const TriMesh& tri_mesh, bool enable_padding)
{
	write_buffer("data/meshes/vertices_" + suffix + ".bin", pad_buffer(tri_mesh.vertices, enable_padding, sizeof(float) * 4));
	write_buffer("data/meshes/normals_" + suffix + ".bin", pad_buffer(tri_mesh.normals, enable_padding, sizeof(float) * 4));
	write_buffer("data/meshes/face_normals_" + suffix + ".bin", pad_buffer(tri_mesh.face_normals, enable_padding, sizeof(float) * 4));
	write_buffer("data/meshes/face_area_" + suffix + ".bin", tri_mesh.face_area);
	write_buffer("data/meshes/indices_" + suffix + ".bin", tri_mesh.indices);
	write_buffer("data/meshes/texcoords_" + suffix + ".bin", pad_buffer(tri_mesh.texcoords, enable_padding, sizeof(float) * 4));
}
} // namespace IO
} // namespace IG