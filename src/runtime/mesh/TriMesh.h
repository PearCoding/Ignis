#pragma once

#include "IG_Config.h"

namespace IG {

template <size_t N>
using StVectorXf = Eigen::Matrix<float, N, 1, Eigen::DontAlign>;
using StVector3f = StVectorXf<3>;
using StVector2f = StVectorXf<2>;

struct TriMesh {
	std::vector<StVector3f> vertices;
	std::vector<uint32> indices; // A triangle is based as [i0,i1,i2,m] with m = Material
	std::vector<StVector3f> normals;
	std::vector<StVector3f> face_normals;
	std::vector<float> face_area;
	std::vector<StVector2f> texcoords;

	inline size_t faceCount() const { return indices.size() / 4; }

	void fixNormals();
	void flipNormals();
	void scale(float scale);
	void mergeFrom(const TriMesh& src);
	void replaceID(uint32 m_idx);

	void computeFaceNormals(size_t first_index = 0);
	void computeVertexNormals(size_t first_index = 0);
	void makeTexCoordsZero();
	void setupFaceNormalsAsVertexNormals();
};

} // namespace IG
