#include "ObjFile.h"
#include "Logger.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

namespace IG {
namespace obj {
// Only partial Wavefront support -> No normal or texcoords indices supported
TriMesh load(const std::filesystem::path& path, size_t mtl_offset)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());

	if (!warn.empty())
		IG_LOG(L_WARNING) << "[Obj] " << warn << std::endl;

	if (!err.empty())
		IG_LOG(L_ERROR) << "[Obj] " << err << std::endl;

	if (!ret)
		return TriMesh{};

	if (attrib.vertices.empty()) {
		IG_LOG(L_ERROR) << "[Obj] No vertices given!" << std::endl;
		return TriMesh{};
	}

	if (shapes.size() > 1)
		IG_LOG(L_WARNING) << "[Obj] Contains multiple shapes. Will be combined to one " << std::endl;

	const bool hasNorms	 = (attrib.normals.size() == attrib.vertices.size());
	const bool hasCoords = (attrib.texcoords.size() / 2 == attrib.vertices.size() / 3);

	size_t indices_count = 0;
	for (size_t sh = 0; sh < shapes.size(); ++sh)
		indices_count += shapes[sh].mesh.indices.size();

	TriMesh tri_mesh;

	// Indices
	tri_mesh.indices.reserve(indices_count * 4);
	for (size_t s = 0; s < shapes.size(); s++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			int fv = shapes[s].mesh.num_face_vertices[f];
			IG_ASSERT(fv == 3, "Expected tinyobjloader generating triangular data!");

			tri_mesh.indices.push_back(shapes[s].mesh.indices[index_offset + 0].vertex_index);
			tri_mesh.indices.push_back(shapes[s].mesh.indices[index_offset + 1].vertex_index);
			tri_mesh.indices.push_back(shapes[s].mesh.indices[index_offset + 2].vertex_index);
			tri_mesh.indices.push_back(shapes[s].mesh.material_ids[f]); // Last entry is material index
			index_offset += fv;
		}
	}

	// Vertices
	tri_mesh.vertices.reserve(attrib.vertices.size() / 3);
	for (size_t v = 0; v < attrib.vertices.size() / 3; ++v)
		tri_mesh.vertices.emplace_back(attrib.vertices[3 * v + 0], attrib.vertices[3 * v + 1], attrib.vertices[3 * v + 2]);

	// Normals
	tri_mesh.computeFaceNormals();
	if (hasNorms) {
		tri_mesh.normals.reserve(attrib.normals.size() / 3);
		for (size_t v = 0; v < attrib.normals.size() / 3; ++v)
			tri_mesh.normals.emplace_back(attrib.normals[3 * v + 0], attrib.normals[3 * v + 1], attrib.normals[3 * v + 2]);

		tri_mesh.fixNormals();
	} else {
		IG_LOG(L_WARNING) << "[Obj] No valid normals given. Recalculating " << std::endl;
		tri_mesh.computeVertexNormals();
	}

	// Texcoords
	if (hasCoords) {
		tri_mesh.texcoords.reserve(tri_mesh.vertices.size());
		for (size_t v = 0; v < tri_mesh.vertices.size(); ++v)
			tri_mesh.texcoords.emplace_back(attrib.texcoords[2 * v + 0], attrib.texcoords[2 * v + 1]);
	} else {
		IG_LOG(L_WARNING) << "[Obj] No texture coordinates are present, using default value." << std::endl;
		tri_mesh.makeTexCoordsZero();
	}

	return tri_mesh;
}
} // namespace obj
} // namespace IG
