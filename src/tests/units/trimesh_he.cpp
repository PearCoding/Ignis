#include "mesh/TriMesh.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace IG;
TEST_CASE("Validate half edge structure for a icosphere", "[TriMesh]")
{
    constexpr float Radius = 4;

    TriMesh mesh = TriMesh::MakeIcoSphere(Vector3f::Zero(), Radius, 4);

    const auto halfedges = mesh.computeHalfEdges();

    REQUIRE(halfedges.size() < mesh.faceCount() * 3 * 2);

    for (uint32 id = 0; id < (uint32)halfedges.size(); ++id) {
        const auto he = halfedges.at(id);
        uint32 face   = id / 3;

        // The twin of the twin is us
        REQUIRE(halfedges[he.Twin].Twin == id);

        // Next and previous half edges share the same face
        REQUIRE(he.Next / 3 == face);
        REQUIRE(he.Previous / 3 == face);

        // The twin of the previous half edge has the same vertex
        REQUIRE(halfedges[halfedges[he.Previous].Twin].Vertex == he.Vertex);
    }
}

TEST_CASE("Validate half edge structure for a triangle", "[TriMesh]")
{
    TriMesh mesh = TriMesh::MakeTriangle(Vector3f::Zero(), Vector3f::UnitX(), Vector3f::UnitY());

    const auto halfedges = mesh.computeHalfEdges();

    REQUIRE(halfedges.size() < mesh.faceCount() * 3 * 2);

    for (uint32 id = 0; id < (uint32)halfedges.size(); ++id) {
        const auto he = halfedges.at(id);
        uint32 face   = id / 3;

        // A triangle has no twin edges
        REQUIRE(he.Twin == NoHalfEdgeTwin);

        // Next and previous half edges share the same face
        REQUIRE(he.Next / 3 == face);
        REQUIRE(he.Previous / 3 == face);
    }
}
