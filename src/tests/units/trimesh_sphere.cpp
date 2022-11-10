#include "mesh/TriMesh.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace IG;
TEST_CASE("Check if a generated ico sphere is detected as sphere", "[TriMesh]")
{
    constexpr float Radius = 4;

    TriMesh mesh = TriMesh::MakeIcoSphere(Vector3f::Zero(), Radius, 4);

    auto sphere = mesh.getAsSphere();
    REQUIRE(sphere.has_value());

    auto shape = sphere.value();

    CHECK_THAT(shape.Origin.x(), Catch::Matchers::WithinRel(0.0f));
    CHECK_THAT(shape.Origin.y(), Catch::Matchers::WithinRel(0.0f));
    CHECK_THAT(shape.Origin.z(), Catch::Matchers::WithinRel(0.0f));

    CHECK_THAT(shape.Radius, Catch::Matchers::WithinRel(Radius));

    CHECK_THAT(shape.computeArea(), Catch::Matchers::WithinRel(4 * Pi * Radius * Radius));
}

TEST_CASE("Check if a generated ico sphere with very low resolution is not detected as sphere", "[TriMesh]")
{
    constexpr float Radius = 4;

    TriMesh mesh = TriMesh::MakeIcoSphere(Vector3f::Zero(), Radius, 0);

    auto sphere = mesh.getAsSphere();
    REQUIRE_FALSE(sphere.has_value());
}

TEST_CASE("Check if a generated uv sphere is detected as sphere", "[TriMesh]")
{
    constexpr float Radius = 4;

    TriMesh mesh = TriMesh::MakeUVSphere(Vector3f::Zero(), Radius, 32, 16);

    auto sphere = mesh.getAsSphere();
    REQUIRE(sphere.has_value());

    auto shape = sphere.value();

    CHECK_THAT(shape.Origin.x(), Catch::Matchers::WithinRel(0.0f));
    CHECK_THAT(shape.Origin.y(), Catch::Matchers::WithinRel(0.0f));
    CHECK_THAT(shape.Origin.z(), Catch::Matchers::WithinRel(0.0f));

    CHECK_THAT(shape.Radius, Catch::Matchers::WithinRel(Radius));

    CHECK_THAT(shape.computeArea(), Catch::Matchers::WithinRel(4 * Pi * Radius * Radius));
}

TEST_CASE("Check if a generated uv sphere with very low resolution is not detected as sphere", "[TriMesh]")
{
    constexpr float Radius = 4;

    TriMesh mesh = TriMesh::MakeUVSphere(Vector3f::Zero(), Radius, 4, 2);

    auto sphere = mesh.getAsSphere();
    REQUIRE_FALSE(sphere.has_value());
}

TEST_CASE("Check if a generated triangle is not detected as sphere", "[TriMesh]")
{
    TriMesh mesh = TriMesh::MakeTriangle(Vector3f::Zero(), Vector3f::UnitX(), Vector3f::UnitY());

    auto sphere = mesh.getAsSphere();
    REQUIRE_FALSE(sphere.has_value());
}

TEST_CASE("Check if a generated cylinder is not detected as sphere", "[TriMesh]")
{
    constexpr float Radius = 4;
    TriMesh mesh = TriMesh::MakeCylinder(Vector3f::Zero(), Radius, Vector3f::UnitZ(), Radius, 32);

    auto sphere = mesh.getAsSphere();
    REQUIRE_FALSE(sphere.has_value());
}