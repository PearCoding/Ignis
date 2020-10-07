#include "IO.h"
#include "mesh/TriMesh.h"
#include "Buffer.h"

namespace IG {
namespace IO {
void write_tri_mesh(const TriMesh& tri_mesh, bool enable_padding)
{
	write_buffer("data/vertices.bin", pad_buffer(tri_mesh.vertices, enable_padding, sizeof(float) * 4));
	write_buffer("data/normals.bin", pad_buffer(tri_mesh.normals, enable_padding, sizeof(float) * 4));
	write_buffer("data/face_normals.bin", pad_buffer(tri_mesh.face_normals, enable_padding, sizeof(float) * 4));
	write_buffer("data/face_area.bin", tri_mesh.face_area);
	write_buffer("data/indices.bin", pad_buffer(tri_mesh.indices, enable_padding, sizeof(uint32_t) * 4));
	write_buffer("data/texcoords.bin", pad_buffer(tri_mesh.texcoords, enable_padding, sizeof(float) * 4));
}
}
} // namespace IG