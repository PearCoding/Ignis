#pragma once

#include "IG_Config.h"

namespace IG {

// We assume Vector2f and Vector3f are not vectorized!

struct TriMesh {
	std::vector<Vector3f> vertices;
	std::vector<uint32> indices; // A triangle is based as [i0,i1,i2,m] with m = Material
	std::vector<Vector3f> normals;
	std::vector<Vector3f> face_normals;
	std::vector<float> face_area;
	std::vector<Vector2f> texcoords;

	void fixNormals();
	void flipNormals();
	void scale(float scale);
	void mergeFrom(const TriMesh& src);
	void replaceMaterial(uint32 m_idx);

	void computeFaceNormals(size_t first_index = 0);
	void computeVertexNormals(size_t first_index = 0);
	void makeTexCoordsZero();
};

void computeFaceNormals(const std::vector<uint32>& indices,
						const std::vector<Vector3f>& vertices,
						std::vector<Vector3f>& face_normals,
						std::vector<float>& face_area,
						size_t first_index);
void computeVertexNormals(const std::vector<uint32>& indices,
						  const std::vector<Vector3f>& face_normals,
						  std::vector<Vector3f>& normals,
						  size_t first_index);
} // namespace IG
