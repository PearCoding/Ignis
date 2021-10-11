#pragma once

#include "IG_Config.h"

namespace IG {

template <size_t N>
using StVectorXf = Eigen::Matrix<float, N, 1, Eigen::DontAlign>;
using StVector3f = StVectorXf<3>;
using StVector2f = StVectorXf<2>;

static_assert(sizeof(StVector3f) == sizeof(float) * 3, "Expected storage vector types to be well sized");
static_assert(sizeof(StVector2f) == sizeof(float) * 2, "Expected storage vector types to be well sized");

struct TriMesh {
	std::vector<StVector3f> vertices;
	std::vector<uint32> indices; // A triangle is based as [i0,i1,i2,m] with m = Material
	std::vector<StVector3f> normals;
	std::vector<StVector3f> face_normals;
	std::vector<float> face_inv_area;
	std::vector<StVector2f> texcoords;

	inline size_t faceCount() const { return indices.size() / 4; }

	void fixNormals(bool* hasBadNormals = nullptr);
	void flipNormals();
	void scale(float scale);
	void mergeFrom(const TriMesh& src);
	void replaceID(uint32 m_idx);

	void computeFaceAreaOnly(bool* hasBadAreas = nullptr);
	void computeFaceNormals(bool* hasBadAreas = nullptr);
	void computeVertexNormals();
	void makeTexCoordsZero();
	void setupFaceNormalsAsVertexNormals();

	static TriMesh MakeSphere(const Vector3f& center, float radius, uint32 stacks, uint32 slices);
	static TriMesh MakeDisk(const Vector3f& center, const Vector3f& normal, float radius, uint32 sections);
	static TriMesh MakePlane(const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis);
	static TriMesh MakeTriangle(const Vector3f& p0, const Vector3f& p1, const Vector3f& p2);
	static TriMesh MakeRectangle(const Vector3f& p0, const Vector3f& p1, const Vector3f& p2, const Vector3f& p3);
	static TriMesh MakeBox(const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis, const Vector3f& zAxis);
	static TriMesh MakeCone(const Vector3f& baseCenter, float baseRadius, const Vector3f& tipPos, uint32 sections, bool fill_cap = true);
	static TriMesh MakeCylinder(const Vector3f& baseCenter, float baseRadius, const Vector3f& topCenter, float topRadius, uint32 sections, bool fill_cap = true);
};

} // namespace IG
