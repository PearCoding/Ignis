#include "TriMesh.h"
#include "Logger.h"
#include "SHA256.h"
#include "math/Spherical.h"
#include "math/Tangent.h"

#include <algorithm>

namespace IG {

/// Returns non-unit normal for a given triangle
static inline Vector3f computeTriangleNormal(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2)
{
    return (v1 - v0).cross(v2 - v0);
}

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

    std::transform(normals.begin(), normals.end(), normals.begin(),
                   [](const StVector3f& n) { return -n; });
}

size_t TriMesh::removeZeroAreaTriangles()
{
    const auto checkGood = [&](size_t i) {
        const Vector3f v0 = vertices[indices[i + 0]];
        const Vector3f v1 = vertices[indices[i + 1]];
        const Vector3f v2 = vertices[indices[i + 2]];
        const Vector3f N  = computeTriangleNormal(v0, v1, v2);
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

    // Finally update indices
    indices = std::move(new_indices);

    return badAreaCount;
}

void TriMesh::computeVertexNormals()
{
    normals.resize(vertices.size());
    std::fill(normals.begin(), normals.end(), StVector3f::Zero());

    const size_t inds = indices.size();
    for (size_t i = 0; i < inds; i += 4) {
        const auto& v0   = vertices[indices[i + 0]];
        const auto& v1   = vertices[indices[i + 1]];
        const auto& v2   = vertices[indices[i + 2]];
        const Vector3f N = computeTriangleNormal(v0, v1, v2).normalized();

        normals[indices[i + 0]] += N;
        normals[indices[i + 1]] += N;
        normals[indices[i + 2]] += N;
    }

    for (auto& n : normals)
        n.normalize();
}

void TriMesh::makeTexCoordsZero()
{
    texcoords.resize(vertices.size());
    std::fill(texcoords.begin(), texcoords.end(), Vector2f::Zero());
}

void TriMesh::makeTexCoordsNormalized()
{
    texcoords.resize(vertices.size());

    BoundingBox bbox = computeBBox();

    for (size_t i = 0; i < vertices.size(); ++i) {
        const auto& v    = vertices.at(i);
        const Vector3f d = bbox.diameter();
        const Vector3f t = v - bbox.min;

        StVector2f p = StVector2f::Zero();
        if (d.x() > FltEps)
            p.x() = t.x() / d.x();
        if (d.y() > FltEps)
            p.y() = t.y() / d.y();

        texcoords[i] = p; // Drop the z coordinate
    }
}

BoundingBox TriMesh::computeBBox() const
{
    BoundingBox bbox = BoundingBox::Empty();
    for (const auto& v : vertices)
        bbox.extend(v);
    return bbox;
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
        const auto& v0   = vertices[3 * f + 0];
        const auto& v1   = vertices[3 * f + 1];
        const auto& v2   = vertices[3 * f + 2];
        const Vector3f N = computeTriangleNormal(v0, v1, v2).normalized();

        new_normals[3 * f + 0] = N;
        new_normals[3 * f + 1] = N;
        new_normals[3 * f + 2] = N;
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
    for (size_t f = 0; f < faceCount(); ++f) {
        const auto& v0 = vertices[indices[4 * f + 0]];
        const auto& v1 = vertices[indices[4 * f + 1]];
        const auto& v2 = vertices[indices[4 * f + 2]];
        area += 0.5f * computeTriangleNormal(v0, v1, v2).norm();
    }
    return area;
}

std::string TriMesh::computeHash() const
{
    SHA256 hash;
    for (const auto& v : vertices) {
        const float x = v.x();
        const float y = v.y();
        const float z = v.z();
        hash.update(reinterpret_cast<const uint8*>(&x), sizeof(x));
        hash.update(reinterpret_cast<const uint8*>(&y), sizeof(y));
        hash.update(reinterpret_cast<const uint8*>(&z), sizeof(z));
    }
    for (const auto& v : normals) {
        const float x = v.x();
        const float y = v.y();
        const float z = v.z();
        hash.update(reinterpret_cast<const uint8*>(&x), sizeof(x));
        hash.update(reinterpret_cast<const uint8*>(&y), sizeof(y));
        hash.update(reinterpret_cast<const uint8*>(&z), sizeof(z));
    }
    for (const auto& v : texcoords) {
        const float x = v.x();
        const float y = v.y();
        hash.update(reinterpret_cast<const uint8*>(&x), sizeof(x));
        hash.update(reinterpret_cast<const uint8*>(&y), sizeof(y));
    }

    hash.update(reinterpret_cast<const uint8*>(indices.data()), indices.size() * sizeof(uint32));
    return hash.final();
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
        const Vector4f w = TransformMatrix * v.homogeneous();
        return w.block<3, 1>(0, 0) / w.w();
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
}

/// @brief Simple pair key. It is associative (a,b) == (b,a)
struct EdgeKey {
    uint32 A;
    uint32 B;

    inline EdgeKey(uint32 a, uint32 b)
        : A(a < b ? a : b)
        , B(a < b ? b : a)
    {
        IG_ASSERT(a != b, "Only valid edges allowed");
    }
};

inline bool operator==(const EdgeKey& a, const EdgeKey& b)
{
    return a.A == b.A && a.B == b.B;
}

struct EdgeKeyHash {
    auto operator()(const EdgeKey& key) const -> size_t
    {
        return std::hash<decltype(EdgeKey::A)>{}(key.A) ^ std::hash<decltype(EdgeKey::B)>{}(key.B);
    }
};

using EdgeMap = std::unordered_map<EdgeKey, uint32, EdgeKeyHash>;
static EdgeMap computeEdgeMap(const std::vector<uint32>& triangleIndices, const std::vector<bool>* mask)
{
    const size_t triangles = triangleIndices.size() / 4;

    EdgeMap edges; // always [e0, e1]...
    edges.reserve(triangles * 3);

    for (size_t t = 0; t < triangles; ++t) {
        if (mask && !mask->at(t))
            continue;

        const uint32 v0 = triangleIndices[t * 4 + 0];
        const uint32 v1 = triangleIndices[t * 4 + 1];
        const uint32 v2 = triangleIndices[t * 4 + 2];

        edges.try_emplace(EdgeKey(v0, v1), (uint32)edges.size());
        edges.try_emplace(EdgeKey(v1, v2), (uint32)edges.size());
        edges.try_emplace(EdgeKey(v2, v0), (uint32)edges.size());
    }

    return edges;
}

static inline std::array<uint32, 6> subdivideQuadToTriangle(uint32 q0, const Vector3f& v0,
                                                            uint32 q1, const Vector3f& v1,
                                                            uint32 q2, const Vector3f& v2,
                                                            uint32 q3, const Vector3f& v3)
{
    // TODO: Would be nice to get rid of the normalization
    const Vector3f e01 = (v1 - v0).normalized();
    const Vector3f e12 = (v2 - v1).normalized();
    const Vector3f e23 = (v3 - v2).normalized();
    const Vector3f e30 = (v0 - v3).normalized();

    const float cosA = std::abs(e01.dot(e30));
    const float cosB = std::abs(e01.dot(e12));
    const float cosC = std::abs(e12.dot(e23));
    const float cosD = std::abs(e23.dot(e30));

    // Configuration 1
    const float c1 = cosA + cosC;
    // Configuration 2
    const float c2 = cosB + cosD;

    // Split the larger sum angle (which is the smaller cosine)
    if (c1 < c2)
        return { q0, q1, q2, q0, q2, q3 };
    else
        return { q0, q1, q3, q1, q2, q3 };
}

void TriMesh::subdivide(const std::vector<bool>* mask)
{
    const bool hasMask = mask && mask->size() == faceCount();
    const auto edges   = computeEdgeMap(indices, hasMask ? mask : nullptr);

    const size_t newEdgeCount          = edges.size();
    const size_t previousVertexCount   = vertices.size();
    const size_t previousTriangleCount = indices.size() / 4;

    // Append new vertices
    vertices.resize(previousVertexCount + newEdgeCount);
    for (const auto& p : edges)
        vertices[previousVertexCount + p.second] = (vertices.at(p.first.A) + vertices.at(p.first.B)) / 2;

    // Append new normals if needed
    if (previousVertexCount == normals.size()) {
        normals.resize(previousVertexCount + newEdgeCount);
        for (const auto& p : edges)
            normals[previousVertexCount + p.second] = (normals.at(p.first.A) + normals.at(p.first.B)).normalized();
    }

    // Append new texcoords if needed
    if (previousVertexCount == texcoords.size()) {
        texcoords.resize(previousVertexCount + newEdgeCount);
        for (const auto& p : edges)
            texcoords[previousVertexCount + p.second] = (texcoords.at(p.first.A) + texcoords.at(p.first.B)) / 2;
    }

    // Setup indices
    std::vector<uint32> newIndices;
    const auto addNewIndices = [&](size_t t) {
        const uint32 v0  = indices[t * 4 + 0];
        const uint32 v1  = indices[t * 4 + 1];
        const uint32 v2  = indices[t * 4 + 2];
        const uint32 e01 = (uint32)previousVertexCount + edges.at(EdgeKey(v0, v1));
        const uint32 e12 = (uint32)previousVertexCount + edges.at(EdgeKey(v1, v2));
        const uint32 e20 = (uint32)previousVertexCount + edges.at(EdgeKey(v2, v0));

        newIndices.insert(newIndices.end(), { v0, e01, e20, 0,
                                              v1, e12, e01, 0,
                                              v2, e20, e12, 0,
                                              e01, e12, e20, 0 });
    };

    if (hasMask) {
        constexpr float InvGoldenRatio = 0.6180339887f;
        // Previous triangles plus a triangle for each split and a reserve of some triangles for dangling cases
        const size_t expectedTriangles = std::min(previousTriangleCount * 4, previousTriangleCount + edges.size() + static_cast<size_t>(edges.size() * InvGoldenRatio));
        newIndices.reserve(expectedTriangles * 4);
        for (size_t t = 0; t < previousTriangleCount; ++t) {
            if (mask->at(t)) {
                addNewIndices(t);
            } else {
                const uint32 v0 = indices[t * 4 + 0];
                const uint32 v1 = indices[t * 4 + 1];
                const uint32 v2 = indices[t * 4 + 2];

                const auto it01 = edges.find(EdgeKey(v0, v1));
                const auto it12 = edges.find(EdgeKey(v1, v2));
                const auto it20 = edges.find(EdgeKey(v2, v0));

                const uint8 bits = (it01 != edges.end() ? 0x1 : 0) | (it12 != edges.end() ? 0x2 : 0) | (it20 != edges.end() ? 0x4 : 0);

                const uint32 e01 = it01 != edges.end() ? (uint32)previousVertexCount + it01->second : 0;
                const uint32 e12 = it12 != edges.end() ? (uint32)previousVertexCount + it12->second : 0;
                const uint32 e20 = it20 != edges.end() ? (uint32)previousVertexCount + it20->second : 0;

                switch (bits) {
                default:
                case 0x0:
                    // No neighboring subdivisions
                    newIndices.insert(newIndices.end(), { v0, v1, v2, 0 });
                    break;
                    // A single neighbor is subdivided, so fix the opposing vertex to prevent cuts
                case 0x1: // e01
                    newIndices.insert(newIndices.end(), { v0, e01, v2, 0,
                                                          v1, v2, e01, 0 });
                    break;
                case 0x2: // e12
                    newIndices.insert(newIndices.end(), { v1, e12, v0, 0,
                                                          v2, v0, e12, 0 });
                    break;
                case 0x4: // e20
                    newIndices.insert(newIndices.end(), { v0, v1, e20, 0,
                                                          v1, v2, e20, 0 });
                    break;
                    // Two neighbors are subdivided, we have to get creative
                case 0x3: { // e01, e12
                    const auto res = subdivideQuadToTriangle(v0, vertices[v0], e01, vertices[e01], e12, vertices[e12], v2, vertices[v2]);
                    newIndices.insert(newIndices.end(), { v0, e01, v2, 0,
                                                          res[0], res[1], res[2], 0,
                                                          res[3], res[4], res[5], 0 });
                } break;
                case 0x5: { // e01, e20
                    const auto res = subdivideQuadToTriangle(e01, vertices[e01], v1, vertices[v1], v2, vertices[v2], e20, vertices[e20]);
                    newIndices.insert(newIndices.end(), { v0, e01, e20, 0,
                                                          res[0], res[1], res[2], 0,
                                                          res[3], res[4], res[5], 0 });
                } break;
                case 0x6: { // e12, e20
                    const auto res = subdivideQuadToTriangle(v0, vertices[v0], v1, vertices[v1], e12, vertices[e12], e20, vertices[e20]);
                    newIndices.insert(newIndices.end(), { v0, v1, e20, 0,
                                                          res[0], res[1], res[2], 0,
                                                          res[3], res[4], res[5], 0 });
                } break;
                    // All neighbors are subdivided, so just subdivide this one as well
                case 0x7: // e01, e12, e20
                    newIndices.insert(newIndices.end(), { v0, e01, e20, 0,
                                                          v1, e12, e01, 0,
                                                          v2, e20, e12, 0,
                                                          e01, e12, e20, 0 });
                    break;
                }
            }
        }
    } else {
        newIndices.reserve(previousTriangleCount * 4 * 4); // Each triangle splits into four triangles, which each is given as a pack of 4 uints
        for (size_t t = 0; t < previousTriangleCount; ++t)
            addNewIndices(t);
    }

    indices = std::move(newIndices);
}

void TriMesh::markAreaGreater(std::vector<bool>& mask, float threshold) const
{
    mask.resize(faceCount(), false);

    const float threshold2 = threshold * threshold;
    for (size_t i = 0; i < faceCount(); ++i) {
        const Vector3f v0 = vertices[indices[4 * i + 0]];
        const Vector3f v1 = vertices[indices[4 * i + 1]];
        const Vector3f v2 = vertices[indices[4 * i + 2]];
        const Vector3f N  = computeTriangleNormal(v0, v1, v2);
        mask[i]           = N.squaredNorm() >= threshold2;
    }
}

void TriMesh::applySkinning(const std::vector<float>& weightsPerVertexPerJoint,
                            const std::vector<uint32>& indexPerVertexPerJoint,
                            const AlignedVector<Matrix4f>& transformsPerJoint,
                            size_t numJointsPerVertex)
{
    if (numJointsPerVertex == 0)
        return;

    if (weightsPerVertexPerJoint.size() / numJointsPerVertex != vertices.size()) {
        IG_LOG(L_ERROR) << "Given weights for skinning do not match the number of vertices multiplied by the number of joints per vertices!" << std::endl;
        return;
    }

    if (weightsPerVertexPerJoint.size() != indexPerVertexPerJoint.size()) {
        IG_LOG(L_ERROR) << "Given weights and indices for skinning are not of the same size!" << std::endl;
        return;
    }

    const bool hasNormals = normals.size() == vertices.size();
    for (size_t i = 0; i < vertices.size(); ++i) {
        Matrix4f transform = Matrix4f::Zero();
        for (size_t j = 0; j < numJointsPerVertex; ++j)
            transform += weightsPerVertexPerJoint[i * numJointsPerVertex + j] * transformsPerJoint.at(indexPerVertexPerJoint[i * numJointsPerVertex + j]);

        Vector4f homogenous = transform * vertices[i].homogeneous();
        vertices[i]         = homogenous.block<3, 1>(0, 0) / homogenous.w();
        if (hasNormals)
            normals[i] = (transform.block<3, 3>(0, 0).transpose().inverse() * normals[i]).normalized();
    }
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
    const Vector3f fn0 = computeTriangleNormal(vertices[indices[0]], vertices[indices[1]], vertices[indices[2]]).normalized();
    const Vector3f fn1 = computeTriangleNormal(vertices[indices[4 + 0]], vertices[indices[4 + 1]], vertices[indices[4 + 2]]).normalized();
    if (!fn0.isApprox(fn1, PlaneEPS))
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
    if (fn0.dot(normal) < 0) {
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

std::optional<SphereShape> TriMesh::getAsSphere() const
{
    constexpr float SphereEPS   = 1e-4f;
    constexpr float Sphere90EPS = 1e-7f;

    // A sphere requires a sufficient amount of resolution. We pick some empirical value
    if (faceCount() < 32)
        return std::nullopt;

    const BoundingBox bbox = computeBBox();
    const Vector3f origin  = bbox.center();

    // Should not be a flat
    if (bbox.volume() <= SphereEPS)
        return std::nullopt;

    // Check if it is symmetric
    const float wh = std::abs((bbox.diameter().x() - bbox.diameter().y()) / std::max(1e-5f, bbox.diameter().x() + bbox.diameter().y()));
    const float wd = std::abs((bbox.diameter().x() - bbox.diameter().z()) / std::max(1e-5f, bbox.diameter().x() + bbox.diameter().z()));
    const float hd = std::abs((bbox.diameter().y() - bbox.diameter().z()) / std::max(1e-5f, bbox.diameter().y() + bbox.diameter().z()));
    if (wh > SphereEPS || wd > SphereEPS || hd > SphereEPS)
        return std::nullopt;

    // Compute squared radius from average
    float radius2 = 0;
    for (const auto& v : vertices)
        radius2 += (origin - v).squaredNorm();
    radius2 /= vertices.size();
    if (radius2 <= SphereEPS) // Points are not spheres... at least not here
        return std::nullopt;

    // Check if all vertices are spread around the origin by the given radius
    for (const auto& v : vertices) {
        const float r2 = (origin - v).squaredNorm();
        if (std::abs(r2 - radius2) > SphereEPS)
            return std::nullopt;
    }

    const auto getFaceNormal = [&](size_t t) {
        const Vector3f v0 = vertices.at(indices.at(t * 4 + 0));
        const Vector3f v1 = vertices.at(indices.at(t * 4 + 1));
        const Vector3f v2 = vertices.at(indices.at(t * 4 + 2));
        return computeTriangleNormal(v0, v1, v2).normalized();
    };

    const auto halfEdges = computeHalfEdges();

    // Some more complex test to ensure not to detect our default cylinder as a sphere.
    for (size_t id = 0; id < halfEdges.size(); ++id) {
        const auto he = halfEdges[id];
        if (he.Twin == NoHalfEdgeTwin)
            continue;

        const Vector3f N1 = getFaceNormal(id / 3);
        if (N1.hasNaN()) // Zero area triangles are always bad
            return std::nullopt;

        const Vector3f N2 = getFaceNormal(he.Twin / 3);
        if (N2.hasNaN()) // Zero area triangles are always bad
            return std::nullopt;

        // Check if any normal is 90° to to its neighbors. This should never be the case for a sphere.
        if (std::abs(N1.dot(N2)) <= Sphere90EPS)
            return std::nullopt;
    }

    // Make sure all sides have geometry present. This is not a perfect solution, but a huge improvement nevertheless
    std::array<bool, 8> sectors;
    std::fill(sectors.begin(), sectors.end(), false);
    for (const auto& v : vertices) {
        const Vector3f d = origin - v;

        size_t id = 0;
        if (d.x() < 0)
            id |= 0x1;
        if (d.y() < 0)
            id |= 0x2;
        if (d.z() < 0)
            id |= 0x4;

        sectors[id] = true;
    }
    for (bool b : sectors) {
        if (!b)
            return std::nullopt;
    }

    // Construct approximative shape
    SphereShape shape;
    shape.Origin = origin;
    shape.Radius = std::sqrt(radius2);

    // The given sphere is only approximative. Texture coordinates are ignored and actual face connectivities (indices) are also neglected
    return shape;
}

std::vector<TriMesh::HalfEdge> TriMesh::computeHalfEdges() const
{
    const size_t triangles = faceCount();

    std::vector<TriMesh::HalfEdge> halfEdges;
    halfEdges.reserve(triangles * 3);

    EdgeMap edges; // always [e0, e1]...
    edges.reserve(triangles * 3);

    for (size_t t = 0; t < triangles; ++t) {
        const uint32 id = (uint32_t)t * 3;
        for (uint32 k = 0; k < 3 /*Triangle*/; ++k) {
            const uint32 v  = indices[t * 4 + k];
            const uint32 vn = indices[t * 4 + (k + 1) % 3];

            HalfEdge he = {
                v,
                NoHalfEdgeTwin,   // Will be set later
                id + (k + 1) % 3, // Next
                id + (k + 2) % 3  // Previous
            };

            halfEdges.emplace_back(he);

            // Setup half edge face twins
            if (const auto it = edges.find(EdgeKey(v, vn)); it != edges.end()) {
                IG_ASSERT(halfEdges[it->second].Vertex == vn, "Expected well oriented vertices");
                halfEdges[id + k].Twin     = it->second;
                halfEdges[it->second].Twin = id + k;
            } else {
                edges.emplace(EdgeKey(v, vn), id + k);
            }
        }
    }

    return halfEdges;
}

inline static void addTriangle(TriMesh& mesh, const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis)
{
    const Vector3f N = xAxis.cross(yAxis).normalized();
    const uint32 off = (uint32)mesh.vertices.size();

    mesh.vertices.insert(mesh.vertices.end(), { origin, origin + xAxis, origin + yAxis });
    mesh.normals.insert(mesh.normals.end(), { N, N, N });
    mesh.texcoords.insert(mesh.texcoords.end(), { Vector2f(0, 0), Vector2f(1, 0), Vector2f(0, 1) });
    mesh.indices.insert(mesh.indices.end(), { 0 + off, 1 + off, 2 + off, 0 });
}

inline static void addGrid(TriMesh& mesh, const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis, uint32 count_x, uint32 count_y)
{
    const Vector3f N = xAxis.cross(yAxis).normalized();
    const uint32 off = (uint32)mesh.vertices.size();

    // Construct vertices
    mesh.vertices.reserve(mesh.vertices.size() + (count_x + 1) * (count_y * 1));
    mesh.normals.reserve(mesh.normals.size() + (count_x + 1) * (count_y * 1));
    mesh.texcoords.reserve(mesh.texcoords.size() + (count_x + 1) * (count_y * 1));
    for (uint32 j = 0; j <= count_y; ++j) {
        for (uint32 i = 0; i <= count_x; ++i) {
            const float u      = i / (float)count_x;
            const float v      = j / (float)count_y;
            const Vector3f pos = origin + xAxis * u + yAxis * v;

            mesh.vertices.emplace_back(pos);
            mesh.normals.emplace_back(N);
            mesh.texcoords.emplace_back(Vector2f(u, v));
        }
    }

    // Setup indices
    for (uint32 j = 0; j < count_y; ++j) {
        for (uint32 i = 0; i < count_x; ++i) {
            const uint32 ind1 = j * (count_x + 1) + i + off;
            const uint32 ind2 = (j + 1) * (count_x + 1) + i + off;
            mesh.indices.insert(mesh.indices.end(), { ind1, ind1 + 1, ind2 + 1, 0, ind1, ind2 + 1, ind2, 0 });
        }
    }
}

inline static void addPlane(TriMesh& mesh, const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis)
{
    addGrid(mesh, origin, xAxis, yAxis, 1, 1);
}

inline static void addDisk(TriMesh& mesh, const Vector3f& origin, const Vector3f& N, const Vector3f& Nx, const Vector3f& Ny,
                           float radius, uint32 sections, bool fill_cap, bool flip = false)
{
    const float step = 1.0f / sections;
    const uint32 off = (uint32)mesh.vertices.size();

    if (fill_cap) {
        mesh.vertices.emplace_back(origin);
        mesh.normals.emplace_back(N);
        mesh.texcoords.emplace_back(Vector2f(0, 0));
    }

    for (uint32 i = 0; i < sections; ++i) {
        const float x = std::cos(2 * Pi * step * i);
        const float y = std::sin(2 * Pi * step * i);

        mesh.vertices.emplace_back(radius * Nx * x + radius * Ny * y + origin);
        mesh.normals.emplace_back(N);
        mesh.texcoords.emplace_back(Vector2f(0.5f * (x + 1), 0.5f * (y + 1)));
    }

    if (!fill_cap)
        return;

    const uint32 start = 1; // Skip disk origin
    for (uint32 i = 0; i < sections; ++i) {
        uint32 C  = i + start;
        uint32 NC = (i + 1 < sections ? i + 1 : 0) + start;
        if (flip)
            mesh.indices.insert(mesh.indices.end(), { 0 + off, NC + off, C + off, 0 });
        else
            mesh.indices.insert(mesh.indices.end(), { 0 + off, C + off, NC + off, 0 });
    }
}

TriMesh TriMesh::MakeUVSphere(const Vector3f& center, float radius, uint32 stacks, uint32 slices)
{
    stacks = std::max<uint32>(2, stacks);
    slices = std::max<uint32>(2, slices);

    TriMesh mesh;

    const uint32 count = slices * stacks;
    mesh.vertices.reserve(count);
    mesh.normals.reserve(count);
    mesh.texcoords.reserve(count);

    float drho   = Pi / (float)stacks;
    float dtheta = 2 * Pi / (float)slices;

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
        }
    }

    // Indices
    mesh.indices.reserve(stacks * slices * 8);
    for (uint32 i = 0; i < stacks; ++i) {
        const uint32 c = (i + 0) * slices;
        const uint32 n = (i + 1) * slices;

        for (uint32 j = 0; j < slices; ++j) {
            const uint32 nj = (j + 1) % slices;
            mesh.indices.insert(mesh.indices.end(), { n + j, n + nj, c + nj, 0,
                                                      n + j, c + nj, c + j, 0 });
        }
    }

    return mesh;
}

// Based on http://twistedoakstudios.com/blog/Post1080_my-bug-my-bad-1-fractal-spheres
TriMesh TriMesh::MakeIcoSphere(const Vector3f& center, float radius, uint32 subdivisions)
{
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
                mesh.indices.insert(mesh.indices.end(), { i1, rev ? i3 : i2, rev ? i2 : i3, 0 });
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
                mesh.indices.insert(mesh.indices.end(), { i1, rev ? i3 : i2, rev ? i2 : i3, 0 });
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
            refinedIndices.insert(refinedIndices.end(), { edgeCenterVertices[0], edgeCenterVertices[1], edgeCenterVertices[2], 0 });
            // Create a triangle for each corner of the existing triangle
            for (auto j = 0; j < 3; j++)
                refinedIndices.insert(refinedIndices.end(), { tri[j], edgeCenterVertices.at((j + 0) % 3), edgeCenterVertices.at((j + 2) % 3), 0 });
        }

        mesh.indices = std::move(refinedIndices);
    }

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
    addDisk(mesh, center, normal, Nx, Ny, radius, sections, true);
    return mesh;
}

TriMesh TriMesh::MakePlane(const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis)
{
    TriMesh mesh;
    addPlane(mesh, origin, xAxis, yAxis);
    return mesh;
}

TriMesh TriMesh::MakeTriangle(const Vector3f& p0, const Vector3f& p1, const Vector3f& p2)
{
    TriMesh mesh;
    addTriangle(mesh, p0, p1 - p0, p2 - p0);
    return mesh;
}

TriMesh TriMesh::MakeRectangle(const Vector3f& p0, const Vector3f& p1, const Vector3f& p2, const Vector3f& p3)
{
    TriMesh mesh;
    addTriangle(mesh, p0, p1 - p0, p3 - p0);
    addTriangle(mesh, p1, p2 - p1, p3 - p1);
    return mesh;
}

TriMesh TriMesh::MakeBox(const Vector3f& origin, const Vector3f& xAxis, const Vector3f& yAxis, const Vector3f& zAxis)
{
    const Vector3f lll = origin;
    const Vector3f hhh = origin + xAxis + yAxis + zAxis;

    TriMesh mesh;
    addPlane(mesh, lll, yAxis, xAxis);
    addPlane(mesh, lll, xAxis, zAxis);
    addPlane(mesh, lll, zAxis, yAxis);
    addPlane(mesh, hhh, -xAxis, -yAxis);
    addPlane(mesh, hhh, -zAxis, -xAxis);
    addPlane(mesh, hhh, -yAxis, -zAxis);

    return mesh;
}

TriMesh TriMesh::MakeCone(const Vector3f& baseCenter, float baseRadius, const Vector3f& tipPos, uint32 sections, bool fill_cap)
{
    sections = std::max<uint32>(3, sections);

    const Vector3f H = (baseCenter - tipPos).normalized();
    Vector3f Nx, Ny;
    Tangent::frame(H, Nx, Ny);

    TriMesh mesh;
    addDisk(mesh, baseCenter, H, Nx, Ny, baseRadius, sections, fill_cap);

    mesh.vertices.emplace_back(tipPos);
    mesh.normals.emplace_back(H);
    mesh.texcoords.emplace_back(Vector2f::Zero());

    const uint32 start = fill_cap ? 1 : 0; // Skip disk origin
    const uint32 tP    = (uint32)mesh.vertices.size() - 1;
    for (uint32 i = 0; i < sections; ++i) {
        uint32 C  = i + start;
        uint32 NC = (i + 1 < sections ? i + 1 : 0) + start;

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

    addDisk(mesh, baseCenter, H, Nx, Ny, baseRadius, sections, fill_cap);
    const uint32 off = (uint32)mesh.vertices.size();
    addDisk(mesh, topCenter, H, Nx, Ny, topRadius, sections, fill_cap, true);

    const uint32 start = fill_cap ? 1 : 0; // Skip disk origin
    for (uint32 i = 0; i < sections; ++i) {
        uint32 C  = i + start;
        uint32 NC = (i + 1 < sections ? i + 1 : 0) + start;

        mesh.indices.insert(mesh.indices.end(), { C, C + off, NC, 0, C + off, NC + off, NC, 0 });
    }

    mesh.computeVertexNormals();
    return mesh;
}

TriMesh TriMesh::MakeRadialGaussian(const Vector3f& origin, const Vector3f& direction, float sigma, float radius_scale, uint32 sections, uint32 slices)
{
    sections = std::max<uint32>(3, sections);
    slices   = std::max<uint32>(2, slices);

    const auto gauss = [=](float r) { return std::exp(-(r * r) / (2 * sigma * sigma)) / (sigma * 2 * Pi); };

    const Vector3f defect = direction * gauss(1);
    const Vector3f peak   = origin + direction * gauss(0) - defect;
    const Vector3f normal = direction.normalized();

    Vector3f Nx, Ny;
    Tangent::frame(normal, Nx, Ny);

    TriMesh mesh;

    // Bottom circle
    addDisk(mesh, origin, normal, Nx, Ny, radius_scale, sections, true);

    // In-betweens
    for (uint32 i = 1; i < slices; ++i) {
        const float radius = 1 - i / (float)slices;
        const float g      = gauss(radius);

        addDisk(mesh, origin + direction * g - defect, normal, Nx, Ny, radius_scale * radius, sections, false);

        const uint32 start = (i - 1) * sections + 1 /* Origin */;
        for (uint32 k = 0; k < sections; ++k) {
            const uint32 C  = k + start;
            const uint32 NC = (k + 1 < sections ? k + 1 : 0) + start;

            mesh.indices.insert(mesh.indices.end(), { C, C + sections, NC, 0,
                                                      C + sections, NC + sections, NC, 0 });
        }
    }

    // Peak
    mesh.vertices.emplace_back(peak);
    mesh.normals.emplace_back(normal);
    mesh.texcoords.emplace_back(Vector2f::Zero());

    for (uint32 i = 0; i < sections; ++i) {
        const uint32 end   = (uint32)mesh.vertices.size() - 1;
        const uint32 start = (slices - 1) * sections + 1 /* Origin */;
        const uint32 C     = i + start;
        const uint32 NC    = (i + 1 < sections ? i + 1 : 0) + start;

        mesh.indices.insert(mesh.indices.end(), { C, end, NC, 0 });
    }
    std::cout << mesh.vertices.size() << " " << mesh.indices.size() << std::endl;

    // Give it a smooth representation (by default)
    mesh.computeVertexNormals();
    return mesh;
}

TriMesh TriMesh::MakeGaussianLobe(const Vector3f& origin, const Vector3f& direction,
                                  const Vector3f& xAxis, const Vector3f& yAxis,
                                  const Matrix2f& cov, uint32 theta_size, uint32 phi_size, float scale)
{
    theta_size = std::max<uint32>(8, theta_size);
    phi_size   = std::max<uint32>(8, phi_size);

    const Vector3f N  = xAxis.cross(yAxis).normalized();
    const Vector3f Nx = xAxis.normalized();
    const Vector3f Ny = yAxis.normalized();

    const float det = std::abs(cov.determinant());
    if (det <= FltEps) {
        IG_LOG(L_ERROR) << "Given gaussian covariance matrix is not positive semi-definit: " << cov << std::endl;
        return TriMesh();
    }

    const float norm                = 1 / (2 * Pi * std::sqrt(det));
    const Matrix2f invCov           = cov.inverse();
    const auto [meanTheta, meanPhi] = Spherical::toThetaPhi(Tangent::toTangentSpace(N, Nx, Ny, direction.normalized()));

    const auto func = [=](float theta, float phi) {
        const Vector2f a = Vector2f(theta - meanTheta, phi - meanPhi);
        return norm * std::exp(-0.5f * a.transpose() * invCov * a);
    };

    TriMesh mesh;
    addGrid(mesh, Vector3f::Zero(), Vector3f::UnitX(), Vector3f::UnitY(), theta_size, phi_size);

    for (uint32 j = 0; j <= phi_size; ++j) {       // [-Pi, Pi]
        for (uint32 i = 0; i <= theta_size; ++i) { // [0, Pi]
            const float phi   = 2 * Pi * (j / (float)phi_size) - Pi;
            const float theta = Pi * (i / (float)theta_size);
            const float value = func(theta, phi);
            const Vector3f u  = Tangent::fromTangentSpace(N, xAxis, yAxis, Spherical::fromThetaPhi(theta, phi));

            mesh.vertices[j * (theta_size + 1) + i] = u * value * scale + origin;
        }
    }

    mesh.computeVertexNormals();
    return mesh;
}

} // namespace IG
