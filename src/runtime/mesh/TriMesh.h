#pragma once

#include "shape/PlaneShape.h"

namespace IG {

template <size_t N>
using StVectorXf = Eigen::Matrix<float, N, 1, Eigen::DontAlign>;
using StVector3f = StVectorXf<3>;
using StVector2f = StVectorXf<2>;

static_assert(sizeof(StVector3f) == sizeof(float) * 3, "Expected storage vector types to be well sized");
static_assert(sizeof(StVector2f) == sizeof(float) * 2, "Expected storage vector types to be well sized");

struct TriMesh {
    std::vector<StVector3f> vertices;
    std::vector<uint32> indices; // A triangle is based as [i0,i1,i2,0]
    std::vector<StVector3f> normals;
    std::vector<StVector3f> face_normals;
    std::vector<float> face_inv_area;
    std::vector<StVector2f> texcoords;

    [[nodiscard]] inline size_t faceCount() const { return indices.size() / 4; }

    void fixNormals(bool* hasBadNormals = nullptr);
    void flipNormals();

    // Remove all triangles with zero area. Will return number of triangles removed
    size_t removeZeroAreaTriangles();

    void computeFaceNormals(bool* hasBadAreas = nullptr);
    void computeVertexNormals();
    void makeTexCoordsZero();
    void setupFaceNormalsAsVertexNormals();
    [[nodiscard]] float computeArea() const;

    void transform(const Transformf& t);

    /// Returns PlaneShape if the given mesh can be approximated as a plane
    [[nodiscard]] std::optional<PlaneShape> getAsPlane() const;

    [[nodiscard]] static TriMesh MakeUVSphere(const Vector3f& center, float radius, uint32 stacks, uint32 slices);
    [[nodiscard]] static TriMesh MakeIcoSphere(const Vector3f& center, float radius, uint32 subdivisions);
    [[nodiscard]] static TriMesh MakeDisk(const Vector3f& center, const Vector3f& normal, float radius, uint32 sections);
    [[nodiscard]] static TriMesh MakePlane(const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis);
    [[nodiscard]] static TriMesh MakeTriangle(const Vector3f& p0, const Vector3f& p1, const Vector3f& p2);
    [[nodiscard]] static TriMesh MakeRectangle(const Vector3f& p0, const Vector3f& p1, const Vector3f& p2, const Vector3f& p3);
    [[nodiscard]] static TriMesh MakeBox(const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis, const Vector3f& zAxis);
    [[nodiscard]] static TriMesh MakeCone(const Vector3f& baseCenter, float baseRadius, const Vector3f& tipPos, uint32 sections, bool fill_cap = true);
    [[nodiscard]] static TriMesh MakeCylinder(const Vector3f& baseCenter, float baseRadius, const Vector3f& topCenter, float topRadius, uint32 sections, bool fill_cap = true);
};

} // namespace IG
