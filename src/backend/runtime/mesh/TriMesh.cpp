#include "TriMesh.h"
#include "math/Tangent.h"

#include <algorithm>

namespace IG {

void TriMesh::fixNormals(bool* hasBadNormals)
{
    // Re-normalize all the values in the OBJ file to handle invalid meshes
    bool bad = false;
    for (auto& n : normals) {
        float len2 = n.squaredNorm();
        if (len2 <= FltEps || std::isnan(len2)) {
            bad = true;
            n   = Vector3f::UnitY();
        } else
            n /= std::sqrt(len2);
    }

    if (hasBadNormals)
        *hasBadNormals = bad;
}

void TriMesh::flipNormals()
{
    // Change orientation of triangle
    const size_t inds = indices.size();
    for (size_t i = 0; i < inds; i += 4)
        std::swap(indices[i + 1], indices[i + 2]);

    std::transform(face_normals.begin(), face_normals.end(), face_normals.begin(),
                   [](const StVector3f& n) { return -n; });

    std::transform(normals.begin(), normals.end(), normals.begin(),
                   [](const StVector3f& n) { return -n; });
}

size_t TriMesh::removeZeroAreaTriangles()
{
    const auto checkGood = [&](size_t i) {
        const Vector3f v0 = vertices[indices[i + 0]];
        const Vector3f v1 = vertices[indices[i + 1]];
        const Vector3f v2 = vertices[indices[i + 2]];
        const Vector3f N  = (v1 - v0).cross(v2 - v0);
        return N.squaredNorm() > FltEps;
    };

    // Count how many triangles are bad
    size_t badAreaCount = 0;

    const size_t inds = indices.size();
    for (size_t i = 0; i < inds; i += 4) {
        if (!checkGood(i))
            ++badAreaCount;
    }

    // If no triangle is bad, our job is done
    if (badAreaCount == 0)
        return 0;

    // Compute new indices
    std::vector<uint32> new_indices;
    new_indices.reserve(indices.size() - badAreaCount * 4);

    for (size_t i = 0; i < inds; i += 4) {
        if (checkGood(i)) {
            new_indices.push_back(indices[i + 0]);
            new_indices.push_back(indices[i + 1]);
            new_indices.push_back(indices[i + 2]);
            new_indices.push_back(indices[i + 3]);
        }
    }

    // Make sure at least a single triangle is always available
    if (new_indices.empty()) {
        badAreaCount--;
        new_indices.push_back(indices[0 + 0]);
        new_indices.push_back(indices[0 + 1]);
        new_indices.push_back(indices[0 + 2]);
        new_indices.push_back(indices[0 + 3]);
    }

    // Update face normals if necessary
    if (!face_normals.empty()) {
        std::vector<StVector3f> new_face_normals;
        new_face_normals.reserve(new_indices.size() / 4);

        for (size_t i = 0; i < inds; i += 4) {
            if (checkGood(i))
                new_face_normals.push_back(face_normals[i / 4]);
        }

        // Make sure at least a single triangle is always available
        if (new_face_normals.empty())
            new_face_normals.push_back(face_normals[0]);

        face_normals = std::move(new_face_normals);
    }

    // Update face area if necessary
    if (!face_inv_area.empty()) {
        std::vector<float> new_face_area;
        new_face_area.reserve(new_indices.size() / 4);

        for (size_t i = 0; i < inds; i += 4) {
            if (checkGood(i))
                new_face_area.push_back(face_inv_area[i / 4]);
        }

        // Make sure at least a single triangle is always available
        if (new_face_area.empty())
            new_face_area.push_back(face_inv_area[0]);

        face_inv_area = std::move(new_face_area);
    }

    // Finally update indices
    indices = std::move(new_indices);

    return badAreaCount;
}

void TriMesh::computeFaceNormals(bool* hasBadAreas)
{
    bool bad = false;
    face_normals.resize(faceCount());
    face_inv_area.resize(faceCount());

    const size_t inds = indices.size();
    for (size_t i = 0; i < inds; i += 4) {
        const auto& v0 = vertices[indices[i + 0]];
        const auto& v1 = vertices[indices[i + 1]];
        const auto& v2 = vertices[indices[i + 2]];
        Vector3f N     = (v1 - v0).cross(v2 - v0);
        float lN       = N.norm();
        if (lN < 1e-5f) {
            lN  = 1.0f;
            N   = Vector3f::UnitZ();
            bad = true;
        }
        face_normals[i / 4]  = N / lN;
        face_inv_area[i / 4] = 1 / (0.5f * lN);
    }

    if (hasBadAreas)
        *hasBadAreas = bad;
}

void TriMesh::computeVertexNormals()
{
    normals.resize(faceCount() * 3);
    std::fill(normals.begin(), normals.end(), StVector3f::Zero());

    const size_t inds = indices.size();
    for (size_t i = 0; i < inds; i += 4) {
        auto& n0      = normals[indices[i + 0]];
        auto& n1      = normals[indices[i + 1]];
        auto& n2      = normals[indices[i + 2]];
        const auto& n = face_normals[i / 4];
        n0 += n;
        n1 += n;
        n2 += n;
    }

    for (auto& n : normals)
        n.normalize();
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
        new_normals[3 * f + 0]      = facenormal;
        new_normals[3 * f + 1]      = facenormal;
        new_normals[3 * f + 2]      = facenormal;
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
    for (uint32 f = 0; f < (uint32)faceCount(); ++f) {
        indices[4 * f + 0] = 3 * f + 0;
        indices[4 * f + 1] = 3 * f + 1;
        indices[4 * f + 2] = 3 * f + 2;
    }
}

float TriMesh::computeArea() const
{
    float area = 0;
    for (float v : face_inv_area)
        area += 1 / v;
    return area;
}

struct TransformCache {
    const Matrix4f TransformMatrix;
    const Matrix3f NormalMatrix;

    inline explicit TransformCache(const Transformf& t)
        : TransformMatrix(t.matrix())
        , NormalMatrix(TransformMatrix.block<3, 3>(0, 0).transpose().inverse())
    {
    }

    [[nodiscard]] inline Vector3f applyTransform(const Vector3f& v) const
    {
        Vector4f w = TransformMatrix * Vector4f(v(0), v(1), v(2), 1.0f);
        w /= w(3);
        return Vector3f(w(0), w(1), w(2));
    }

    [[nodiscard]] inline Vector3f applyNormal(const Vector3f& n) const
    {
        return (NormalMatrix * n).normalized();
    }
};

void TriMesh::transform(const Transformf& t)
{
    const TransformCache transform = TransformCache(t);
    if (transform.TransformMatrix.isIdentity())
        return;

    for (auto& v : vertices)
        v = transform.applyTransform(v);
    for (auto& n : normals)
        n = transform.applyNormal(n);

    computeFaceNormals();
}

std::optional<PlaneShape> TriMesh::getAsPlane() const
{
    constexpr float PlaneEPS = 1e-5f;

    // A plane is given by two triangles
    if (faceCount() != 2)
        return std::nullopt;

    // We only allow planes given by four points
    std::array<Vector3f, 4> unique_verts;
    std::array<uint32, 4> unique_ids;
    if (vertices.size() != 4) {
        // Further check, maybe the points are not handled as unique
        if (vertices.size() > 4 && vertices.size() <= 6) {
            size_t i  = 0;
            uint32 id = 0;
            for (const auto& v : vertices) {
                bool found = false;
                for (const auto& u : unique_verts) {
                    found = v.isApprox(u, PlaneEPS);
                    if (found)
                        break;
                }
                if (!found) {
                    if (i >= 3)
                        return std::nullopt;
                    unique_ids[i + 1]   = id;
                    unique_verts[i + 1] = v;
                    ++i;
                }
                ++id;
            }
            if (i != 4)
                return std::nullopt;
        } else {
            return std::nullopt;
        }
    } else {
        for (size_t i = 0; i < unique_verts.size(); ++i) {
            unique_verts[i] = vertices[i];
            unique_ids[i]   = (uint32)i;
        }
    }

    // If the two triangles are facing differently, its not a plane
    if (!face_normals[0].isApprox(face_normals[1], PlaneEPS))
        return std::nullopt;

    // Check each edge. The second triangle has to have the same edges (but order might be different)
    float e1 = (vertices[indices[0]] - vertices[indices[1]]).squaredNorm();
    float e2 = (vertices[indices[1]] - vertices[indices[2]]).squaredNorm();
    float e3 = (vertices[indices[2]] - vertices[indices[0]]).squaredNorm();
    float e4 = (vertices[indices[4 + 0]] - vertices[indices[4 + 1]]).squaredNorm();
    float e5 = (vertices[indices[4 + 1]] - vertices[indices[4 + 2]]).squaredNorm();
    float e6 = (vertices[indices[4 + 2]] - vertices[indices[4 + 0]]).squaredNorm();

    const auto safeCheck = [=](float a, float b) -> bool { return std::abs(a - b) <= PlaneEPS; };
    if (!safeCheck(e1, e4) && !safeCheck(e2, e4) && !safeCheck(e3, e4))
        return std::nullopt;
    if (!safeCheck(e1, e5) && !safeCheck(e2, e5) && !safeCheck(e3, e5))
        return std::nullopt;
    if (!safeCheck(e1, e6) && !safeCheck(e2, e6) && !safeCheck(e3, e6))
        return std::nullopt;

    // Find the opposite vertex to origin
    const Vector3f origin = unique_verts[0];
    auto computeAngle     = [&](size_t start) {
        Vector3f x_axis = (unique_verts[(start + 0) % 3 + 1] - origin).normalized();
        Vector3f y_axis = (unique_verts[(start + 1) % 3 + 1] - origin).normalized();
        return std::acos(x_axis.dot(y_axis));
    };

    float angle_12 = std::abs(computeAngle(0));
    float angle_23 = std::abs(computeAngle(1));
    float angle_31 = std::abs(computeAngle(2));

    int sel = 0;
    if (angle_12 >= angle_23 && angle_12 >= angle_31)
        sel = 0;
    else if (angle_23 >= angle_31 && angle_23 >= angle_12)
        sel = 1;
    else
        sel = 2;

    // Populate shape
    PlaneShape shape;
    shape.Origin = origin;
    shape.XAxis  = (unique_verts[(sel + 0) % 3 + 1] - origin);
    shape.YAxis  = (unique_verts[(sel + 1) % 3 + 1] - origin);

    // Check if the sign is flipped
    Vector3f normal = shape.computeNormal();
    if (face_normals[0].dot(normal) < 0) {
        std::swap(shape.XAxis, shape.YAxis);
        std::swap(unique_verts[1], unique_verts[2]);
        std::swap(unique_ids[1], unique_ids[2]);
    }

    // Populate texture coordinates
    if (!texcoords.empty()) {
        shape.TexCoords[0]                 = texcoords[unique_ids[0]];
        shape.TexCoords[(0 + sel) % 3 + 1] = texcoords[unique_ids[1]];
        shape.TexCoords[(1 + sel) % 3 + 1] = texcoords[unique_ids[2]];
        shape.TexCoords[(2 + sel) % 3 + 1] = texcoords[unique_ids[3]];
    } else {
        shape.TexCoords[0] = Vector2f(0, 0);
        shape.TexCoords[1] = Vector2f(1, 0);
        shape.TexCoords[2] = Vector2f(0, 1);
        shape.TexCoords[3] = Vector2f(1, 1);
    }

    return shape;
}

inline static void addTriangle(TriMesh& mesh, const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis, uint32 off)
{
    constexpr uint32 M = 0;
    const Vector3f lN  = xAxis.cross(yAxis);
    const float area   = lN.norm() / 2;
    const Vector3f N   = lN / (2 * area);

    mesh.vertices.insert(mesh.vertices.end(), { origin, origin + xAxis, origin + yAxis });
    mesh.normals.insert(mesh.normals.end(), { N, N, N });
    mesh.texcoords.insert(mesh.texcoords.end(), { Vector2f(0, 0), Vector2f(1, 0), Vector2f(0, 1) });
    mesh.face_normals.insert(mesh.face_normals.end(), N);
    mesh.face_inv_area.insert(mesh.face_inv_area.end(), 1 / area);
    mesh.indices.insert(mesh.indices.end(), { 0 + off, 1 + off, 2 + off, M });
}

inline static void addPlane(TriMesh& mesh, const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis, uint32 off)
{
    constexpr uint32 M = 0;
    const Vector3f lN  = xAxis.cross(yAxis);
    const float area   = lN.norm() / 2;
    const Vector3f N   = lN / (2 * area);

    mesh.vertices.insert(mesh.vertices.end(), { origin, origin + xAxis, origin + xAxis + yAxis, origin + yAxis });
    mesh.normals.insert(mesh.normals.end(), { N, N, N, N });
    mesh.texcoords.insert(mesh.texcoords.end(), { Vector2f(0, 0), Vector2f(1, 0), Vector2f(1, 1), Vector2f(0, 1) });
    mesh.face_normals.insert(mesh.face_normals.end(), { N, N });
    mesh.face_inv_area.insert(mesh.face_inv_area.end(), { 1 / area, 1 / area });
    mesh.indices.insert(mesh.indices.end(), { 0 + off, 1 + off, 2 + off, M, 0 + off, 2 + off, 3 + off, M });
}

inline static void addDisk(TriMesh& mesh, const Vector3f& origin, const Vector3f& N, const Vector3f& Nx, const Vector3f& Ny,
                           float radius, uint32 sections, uint32 off, bool fill_cap)
{
    constexpr uint32 M = 0;
    const float step   = 1.0f / sections;

    if (fill_cap) {
        mesh.vertices.insert(mesh.vertices.end(), origin);
        mesh.normals.insert(mesh.normals.end(), N);
        mesh.texcoords.insert(mesh.texcoords.end(), Vector2f(0, 0));
    }

    for (uint32 i = 0; i < sections; ++i) {
        const float x = std::cos(2 * Pi * step * i);
        const float y = std::sin(2 * Pi * step * i);

        mesh.vertices.insert(mesh.vertices.end(), { radius * Nx * x + radius * Ny * y + origin });
        mesh.normals.insert(mesh.normals.end(), N);
        mesh.texcoords.insert(mesh.texcoords.end(), Vector2f(0.5f * (x + 1), 0.5f * (y + 1)));
    }

    if (!fill_cap)
        return;

    const float area    = 2 * Pi * radius;
    const float secArea = area / sections;
    const uint32 start  = 1; // Skip disk origin
    for (uint32 i = 0; i < sections; ++i) {
        uint32 C  = i + start;
        uint32 NC = (i + 1 < sections ? i + 1 : 0) + start;
        mesh.face_normals.insert(mesh.face_normals.end(), N);
        mesh.face_inv_area.insert(mesh.face_inv_area.end(), 1 / secArea);
        mesh.indices.insert(mesh.indices.end(), { 0 + off, C + off, NC + off, M });
    }
}

TriMesh TriMesh::MakeUVSphere(const Vector3f& center, float radius, uint32 stacks, uint32 slices)
{
    constexpr uint32 M = 0;
    TriMesh mesh;

    const uint32 count = slices * stacks;
    mesh.vertices.reserve(count);
    mesh.normals.reserve(count);
    mesh.texcoords.reserve(count);

    float drho   = 3.141592f / (float)stacks;
    float dtheta = 2 * 3.141592f / (float)slices;

    const float area     = 4 * Pi * radius * radius / (stacks * slices); // TODO: Really?
    const float inv_area = 1 / area;

    // TODO: We create a 2*stacks of redundant vertices at the two critical points... remove them
    // Vertices
    for (uint32 i = 0; i <= stacks; ++i) {
        float rho  = (float)i * drho;
        float srho = std::sin(rho);
        float crho = std::cos(rho);

        for (uint32 j = 0; j < slices; ++j) {
            float theta  = (j == slices) ? 0.0f : j * dtheta;
            float stheta = -std::sin(theta);
            float ctheta = std::cos(theta);

            float x = stheta * srho;
            float y = ctheta * srho;
            float z = crho;

            const Vector3f N = Vector3f(x, y, z);

            mesh.vertices.emplace_back(N * radius + center);
            mesh.normals.emplace_back(N);
            mesh.texcoords.emplace_back(0.5 * theta / Pi, rho / Pi);

            mesh.face_normals.insert(mesh.face_normals.end(), { N, N });
            mesh.face_inv_area.insert(mesh.face_inv_area.end(), { inv_area, inv_area });
        }
    }

    // Indices
    for (uint32 i = 0; i <= stacks; ++i) {
        const uint32 currSliceOff = i * slices;
        const uint32 nextSliceOff = ((i + 1) % (stacks + 1)) * slices;

        for (uint32 j = 0; j < slices; ++j) {
            const uint32 nextJ = ((j + 1) % slices);
            const uint32 id0   = currSliceOff + j;
            const uint32 id1   = currSliceOff + nextJ;
            const uint32 id2   = nextSliceOff + j;
            const uint32 id3   = nextSliceOff + nextJ;

            mesh.indices.insert(mesh.indices.end(), { id2, id3, id1, M, id2, id1, id0, M });
        }
    }

    return mesh;
}

// Based on http://twistedoakstudios.com/blog/Post1080_my-bug-my-bad-1-fractal-spheres
TriMesh TriMesh::MakeIcoSphere(const Vector3f& center, float radius, uint32 subdivisions)
{
    constexpr uint32 M = 0;
    TriMesh mesh;

    // Create vertices (4 per axis plane)
    constexpr float GoldenRatio = 1.618033989f;
    for (int d = 0; d < 3; d++) {
        for (int s1 = -1; s1 <= +1; s1 += 2) {
            for (int s2 = -1; s2 <= +1; s2 += 2) {
                StVector3f vec   = StVector3f::Zero();
                vec[(d + 1) % 3] = GoldenRatio * s1;
                vec[(d + 2) % 3] = 1.0f * s2;
                mesh.vertices.emplace_back(vec.normalized());
            }
        }
    }

    // Create the triangles that have a point on each of the three axis planes
    const auto get_index = [](int d, int s1, int s2) { return d * 4 + (s1 + 1) + ((s2 + 1) >> 1); };
    for (int s1 = -1; s1 <= +1; s1 += 2) {
        for (int s2 = -1; s2 <= +1; s2 += 2) {
            for (int s3 = -1; s3 <= +1; s3 += 2) {
                int rev   = s1 * s2 * s3 == -1;
                uint32 i1 = (uint32)get_index(0, s1, s2);
                uint32 i2 = (uint32)get_index(1, s2, s3);
                uint32 i3 = (uint32)get_index(2, s3, s1);
                mesh.indices.insert(mesh.indices.end(), { i1, rev ? i3 : i2, rev ? i2 : i3, M });
            }
        }
    }

    // Create the triangles that have two points on one axis plane and one point on another
    for (int d = 0; d < 3; d++) {
        for (int s1 = -1; s1 <= +1; s1 += 2) {
            for (int s2 = -1; s2 <= +1; s2 += 2) {
                auto rev = s1 * s2 == +1;
                auto i2  = (uint32)get_index(d, s1, -1);
                auto i1  = (uint32)get_index(d, s1, +1);
                auto i3  = (uint32)get_index((d + 2) % 3, s2, s1);
                mesh.indices.insert(mesh.indices.end(), { i1, rev ? i3 : i2, rev ? i2 : i3, M });
            }
        }
    }

    // Refine
    for (uint32 i = 0; i < subdivisions; ++i) {
        // Place new vertices at centers of spherical edges between existing vertices
        std::unordered_map<uint32, uint32> edgeVertexMap;
        const uint32 prev_size = (uint32)mesh.vertices.size();
        for (auto tri = mesh.indices.begin(); tri != mesh.indices.end(); tri += 4) {
            for (auto j = 0; j < 3; j++) {
                uint32 i1 = tri[j];
                uint32 i2 = tri[(j + 1) % 3];
                if (i1 >= i2)
                    continue; // avoid adding the same edge vertex twice (once from X to Y and once from Y and X)
                uint32 undirectedEdgeId = i1 * prev_size + i2;

                edgeVertexMap[undirectedEdgeId] = (uint32)mesh.vertices.size();
                mesh.vertices.emplace_back((mesh.vertices[i1] + mesh.vertices[i2]).normalized());
            }
        }

        // Create new triangles using old and new vertices
        std::vector<uint32> refinedIndices;
        for (auto tri = mesh.indices.begin(); tri != mesh.indices.end(); tri += 4) {
            // Find vertices at the center of each of the triangle's edges
            std::array<uint32, 3> edgeCenterVertices;
            for (auto j = 0; j < 3; j++) {
                uint32 i1               = tri[j];
                uint32 i2               = tri[(j + 1) % 3];
                uint32 undirectedEdgeId = std::min(i1, i2) * prev_size + std::max(i1, i2);
                edgeCenterVertices[j]   = edgeVertexMap[undirectedEdgeId];
            }

            // Create a triangle covering the center, touching the three edges
            refinedIndices.insert(refinedIndices.end(), { edgeCenterVertices[0], edgeCenterVertices[1], edgeCenterVertices[2], M });
            // Create a triangle for each corner of the existing triangle
            for (auto j = 0; j < 3; j++)
                refinedIndices.insert(refinedIndices.end(), { tri[j], edgeCenterVertices.at((j + 0) % 3), edgeCenterVertices.at((j + 2) % 3), M });
        }

        mesh.indices = std::move(refinedIndices);
    }

    mesh.computeFaceNormals();

    // Vertices are layed out in spherical orientation, use this for the normals
    mesh.normals.resize(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); ++i)
        mesh.normals[i] = mesh.vertices[i].normalized();

    // Compute tex coords based on spherical mapping
    mesh.texcoords.resize(mesh.normals.size());
    for (size_t i = 0; i < mesh.normals.size(); ++i) {
        const auto& N = mesh.normals[i];

        float theta = std::acos(N.z());
        float phi   = std::atan2(-N.x(), N.y());

        if (phi < 0)
            phi += 2 * Pi;

        float u = phi / (2 * Pi);
        float v = theta / Pi;

        mesh.texcoords[i] = StVector2f(u, v);
    }

    // Apply transformation
    Transformf transform = Transformf::Identity();
    if (!center.isZero())
        transform.translate(center);
    if (radius != 1)
        transform.scale(radius);
    mesh.transform(transform);

    return mesh;
}

TriMesh TriMesh::MakeDisk(const Vector3f& center, const Vector3f& normal, float radius, uint32 sections)
{
    sections = std::max<uint32>(3, sections);

    Vector3f Nx, Ny;
    Tangent::frame(normal, Nx, Ny);

    TriMesh mesh;
    addDisk(mesh, center, normal, Nx, Ny, radius, sections, 0, true);
    return mesh;
}

TriMesh TriMesh::MakePlane(const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis)
{
    TriMesh mesh;
    addPlane(mesh, origin, xAxis, yAxis, 0);
    return mesh;
}

TriMesh TriMesh::MakeTriangle(const Vector3f& p0, const Vector3f& p1, const Vector3f& p2)
{
    TriMesh mesh;
    addTriangle(mesh, p0, p1 - p0, p2 - p0, 0);
    return mesh;
}

TriMesh TriMesh::MakeRectangle(const Vector3f& p0, const Vector3f& p1, const Vector3f& p2, const Vector3f& p3)
{
    TriMesh mesh;
    addTriangle(mesh, p0, p1 - p0, p3 - p0, 0);
    addTriangle(mesh, p1, p2 - p1, p3 - p1, 3);
    return mesh;
}

TriMesh TriMesh::MakeBox(const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis, const Vector3f& zAxis)
{
    const Vector3f lll = origin;
    const Vector3f hhh = origin + xAxis + yAxis + zAxis;

    TriMesh mesh;
    addPlane(mesh, lll, yAxis, xAxis, 0);
    addPlane(mesh, lll, xAxis, zAxis, 4);
    addPlane(mesh, lll, zAxis, yAxis, 8);
    addPlane(mesh, hhh, -xAxis, -yAxis, 12);
    addPlane(mesh, hhh, -zAxis, -xAxis, 16);
    addPlane(mesh, hhh, -yAxis, -zAxis, 20);

    return mesh;
}

TriMesh TriMesh::MakeCone(const Vector3f& baseCenter, float baseRadius, const Vector3f& tipPos, uint32 sections, bool fill_cap)
{
    sections = std::max<uint32>(3, sections);

    const Vector3f H = (baseCenter - tipPos).normalized();
    Vector3f Nx, Ny;
    Tangent::frame(H, Nx, Ny);

    TriMesh mesh;
    addDisk(mesh, baseCenter, H, Nx, Ny, baseRadius, sections, 0, fill_cap);

    mesh.vertices.emplace_back(tipPos);
    mesh.normals.emplace_back(H);
    mesh.texcoords.emplace_back(Vector2f::Zero());

    const uint32 start = fill_cap ? 1 : 0; // Skip disk origin
    const uint32 tP    = (uint32)mesh.vertices.size() - 1;
    for (uint32 i = 0; i < sections; ++i) {
        uint32 C  = i + start;
        uint32 NC = (i + 1 < sections ? i + 1 : 0) + start;

        const Vector3f dx = mesh.vertices[NC] - mesh.vertices[C];
        const Vector3f dy = tipPos - mesh.vertices[C];
        const Vector3f lN = -dx.cross(dy);
        const float area  = lN.norm() / 2;
        const Vector3f N  = lN / (2 * area);

        mesh.face_normals.insert(mesh.face_normals.end(), N);
        mesh.face_inv_area.insert(mesh.face_inv_area.end(), 1 / area);
        mesh.indices.insert(mesh.indices.end(), { C, tP, NC, 0 });
    }

    mesh.computeVertexNormals();
    return mesh;
}

TriMesh TriMesh::MakeCylinder(const Vector3f& baseCenter, float baseRadius, const Vector3f& topCenter, float topRadius, uint32 sections, bool fill_cap)
{
    sections = std::max<uint32>(3, sections);

    const Vector3f H = (baseCenter - topCenter).normalized();
    Vector3f Nx, Ny;
    Tangent::frame(H, Nx, Ny);

    TriMesh mesh;
    const uint32 off = fill_cap ? sections + 1 : sections;
    addDisk(mesh, baseCenter, H, Nx, Ny, baseRadius, sections, 0, fill_cap);
    addDisk(mesh, topCenter, -H, Nx, Ny, topRadius, sections, off, fill_cap);

    const uint32 start = fill_cap ? 1 : 0; // Skip disk origin
    for (uint32 i = 0; i < sections; ++i) {
        uint32 C  = i + start;
        uint32 NC = (i + 1 < sections ? i + 1 : 0) + start;

        const Vector3f dx = mesh.vertices[NC] - mesh.vertices[C];
        const Vector3f dy = mesh.vertices[C + off] - mesh.vertices[C];
        const Vector3f lN = -dx.cross(dy);
        const float area  = lN.norm() / 2;
        const Vector3f N  = lN / (2 * area);

        mesh.face_normals.insert(mesh.face_normals.end(), { N, N });
        mesh.face_inv_area.insert(mesh.face_inv_area.end(), { 1 / area, 1 / area });
        mesh.indices.insert(mesh.indices.end(), { C, C + off, NC, 0, C + off, NC + off, NC, 0 });
    }

    mesh.computeVertexNormals();
    return mesh;
}

} // namespace IG
