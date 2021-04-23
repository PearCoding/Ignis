#include "TriMesh.h"
#include "Logger.h"

namespace IG {

void computeFaceNormals(const std::vector<uint32>& indices,
						const std::vector<StVector3f>& vertices,
						std::vector<StVector3f>& face_normals,
						std::vector<float>& face_area,
						size_t first_index)
{
	bool hasBadArea = false;
	for (auto i = first_index, k = indices.size(); i < k; i += 4) {
		const auto& v0	 = vertices[indices[i + 0]];
		const auto& v1	 = vertices[indices[i + 1]];
		const auto& v2	 = vertices[indices[i + 2]];
		const Vector3f N = (v1 - v0).cross(v2 - v0);
		float lN		 = N.norm();
		if (lN < 0.00000001f) {
			lN		   = 1.0f;
			hasBadArea = true;
		}
		face_normals[i / 4] = N / lN;
		face_area[i / 4]	= 0.5f * lN;
	}

	if (hasBadArea)
		IG_LOG(L_WARNING) << "Triangle mesh contains triangles with zero area" << std::endl;
}

void computeVertexNormals(const std::vector<uint32_t>& indices,
						  const std::vector<StVector3f>& face_normals,
						  std::vector<StVector3f>& normals,
						  size_t first_index)
{
	for (auto i = first_index, k = indices.size(); i < k; i += 4) {
		auto& n0	  = normals[indices[i + 0]];
		auto& n1	  = normals[indices[i + 1]];
		auto& n2	  = normals[indices[i + 2]];
		const auto& n = face_normals[i / 4];
		n0 += n;
		n1 += n;
		n2 += n;
	}
}

void TriMesh::fixNormals()
{
	// Re-normalize all the values in the OBJ file to handle invalid meshes
	bool fixed_normals = false;
	for (auto& n : normals) {
		float len2 = n.squaredNorm();
		if (len2 <= FltEps || std::isnan(len2)) {
			fixed_normals = true;
			n			  = Vector3f::UnitY();
		} else
			n /= std::sqrt(len2);
	}

	if (fixed_normals)
		IG_LOG(L_WARNING) << "Some normals were incorrect and thus had to be replaced with arbitrary values." << std::endl;
}

void TriMesh::flipNormals()
{
	for (auto& n : face_normals)
		n = -n;
	for (auto& n : normals)
		n = -n;
}

void TriMesh::scale(float scale)
{
	for (auto& v : vertices)
		v *= scale;
}

void TriMesh::mergeFrom(const TriMesh& src)
{
	size_t idx_offset = indices.size();
	size_t vtx_offset = vertices.size();

	if (idx_offset == 0) {
		*this = src;
		return;
	}

	vertices.insert(vertices.end(), src.vertices.begin(), src.vertices.end());
	normals.insert(normals.end(), src.normals.begin(), src.normals.end());
	texcoords.insert(texcoords.end(), src.texcoords.begin(), src.texcoords.end());
	face_normals.insert(face_normals.end(), src.face_normals.begin(), src.face_normals.end());
	face_area.insert(face_area.end(), src.face_area.begin(), src.face_area.end());

	indices.resize(idx_offset + src.indices.size());
	for (size_t i = 0; i < src.indices.size(); i += 4) {
		indices[idx_offset + i + 0] = src.indices[i + 0] + vtx_offset;
		indices[idx_offset + i + 1] = src.indices[i + 1] + vtx_offset;
		indices[idx_offset + i + 2] = src.indices[i + 2] + vtx_offset;
		indices[idx_offset + i + 3] = src.indices[i + 3]; // ID
	}
}

void TriMesh::replaceID(uint32 m_idx)
{
	for (size_t i = 0; i < indices.size(); i += 4)
		indices[i + 3] = m_idx; // ID
}

void TriMesh::computeFaceNormals(size_t first_index)
{
	face_normals.resize(faceCount());
	face_area.resize(faceCount());
	IG::computeFaceNormals(indices, vertices, face_normals, face_area, first_index);
}

void TriMesh::computeVertexNormals(size_t first_index)
{
	normals.resize(faceCount() * 3);
	IG::computeVertexNormals(indices, face_normals, normals, first_index);
}

void TriMesh::makeTexCoordsZero()
{
	texcoords.resize(vertices.size());
	std::fill(texcoords.begin(), texcoords.end(), Vector2f::Zero());
}

void TriMesh::setupFaceNormalsAsVertexNormals()
{
	// Copy triangle vertices such that each face is unique
	std::vector<StVector3f> new_vertices;
	new_vertices.resize(faceCount() * 3);
	for (size_t f = 0; f < faceCount(); ++f) {
		new_vertices[3 * f + 0] = vertices[indices[4 * f + 0]];
		new_vertices[3 * f + 1] = vertices[indices[4 * f + 1]];
		new_vertices[3 * f + 2] = vertices[indices[4 * f + 2]];
	}
	vertices = std::move(new_vertices);

	// Use face normals for each vertex
	std::vector<StVector3f> new_normals;
	new_normals.resize(faceCount() * 3);
	for (size_t f = 0; f < faceCount(); ++f) {
		const StVector3f facenormal = face_normals[f];
		new_normals[3 * f + 0]		= facenormal;
		new_normals[3 * f + 1]		= facenormal;
		new_normals[3 * f + 2]		= facenormal;
	}
	normals = std::move(new_normals);

	// Copy texcoords if necessary
	if (!texcoords.empty()) {
		std::vector<StVector2f> new_texcoords;
		new_texcoords.resize(faceCount() * 3);
		for (size_t f = 0; f < faceCount(); ++f) {
			new_texcoords[3 * f + 0] = texcoords[indices[4 * f + 0]];
			new_texcoords[3 * f + 1] = texcoords[indices[4 * f + 1]];
			new_texcoords[3 * f + 2] = texcoords[indices[4 * f + 2]];
		}
		texcoords = std::move(new_texcoords);
	}

	// Setup new indexing list
	for (size_t f = 0; f < faceCount(); ++f) {
		indices[4 * f + 0] = 3 * f + 0;
		indices[4 * f + 1] = 3 * f + 1;
		indices[4 * f + 2] = 3 * f + 2;
	}
}

} // namespace IG
