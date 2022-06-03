#include "mesh/TriMesh.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace IG;
TEST_CASE("Check if a generated plane is detected as plane", "[TriMesh]")
{
    TriMesh mesh = TriMesh::MakePlane(Vector3f::Zero(), Vector3f::UnitX(), Vector3f::UnitY());

    auto plane = mesh.getAsPlane();
    REQUIRE(plane.has_value());

    auto shape = plane.value();

    CHECK_THAT(shape.Origin.x(), Catch::Matchers::WithinRel(0.0f));
    CHECK_THAT(shape.Origin.y(), Catch::Matchers::WithinRel(0.0f));
    CHECK_THAT(shape.Origin.z(), Catch::Matchers::WithinRel(0.0f));

    CHECK_THAT(shape.XAxis.x(), Catch::Matchers::WithinRel(1.0f));
    CHECK_THAT(shape.XAxis.y(), Catch::Matchers::WithinRel(0.0f));
    CHECK_THAT(shape.XAxis.z(), Catch::Matchers::WithinRel(0.0f));

    CHECK_THAT(shape.YAxis.x(), Catch::Matchers::WithinRel(0.0f));
    CHECK_THAT(shape.YAxis.y(), Catch::Matchers::WithinRel(1.0f));
    CHECK_THAT(shape.YAxis.z(), Catch::Matchers::WithinRel(0.0f));

    CHECK_THAT(shape.computeArea(), Catch::Matchers::WithinRel(1.0f));

    CHECK_THAT(shape.computeNormal().x(), Catch::Matchers::WithinRel(0.0f));
    CHECK_THAT(shape.computeNormal().y(), Catch::Matchers::WithinRel(0.0f));
    CHECK_THAT(shape.computeNormal().z(), Catch::Matchers::WithinRel(1.0f));
}

TEST_CASE("Check if a generated triangle is not detected as plane", "[TriMesh]")
{
    TriMesh mesh = TriMesh::MakeTriangle(Vector3f::Zero(), Vector3f::UnitX(), Vector3f::UnitY());

    auto plane = mesh.getAsPlane();
    REQUIRE_FALSE(plane.has_value());
}