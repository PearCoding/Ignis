#include "skysun/ElevationAzimuth.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace IG;
TEST_CASE("Check directional mapping of elevation azimuth YUp", "[ElevationAzimuth]")
{
    ElevationAzimuth az1{ 0, 0 };
    ElevationAzimuth az2{ 1, 1 };
    ElevationAzimuth az3{ 0, -Pi };

    CHECK_THAT(ElevationAzimuth::fromDirectionYUp(az1.toDirectionYUp()).Azimuth, Catch::Matchers::WithinRel(az1.Azimuth));
    CHECK_THAT(ElevationAzimuth::fromDirectionYUp(az1.toDirectionYUp()).Elevation, Catch::Matchers::WithinRel(az1.Elevation));
    CHECK_THAT(ElevationAzimuth::fromDirectionYUp(az2.toDirectionYUp()).Azimuth, Catch::Matchers::WithinRel(az2.Azimuth));
    CHECK_THAT(ElevationAzimuth::fromDirectionYUp(az2.toDirectionYUp()).Elevation, Catch::Matchers::WithinRel(az2.Elevation));
    CHECK_THAT(ElevationAzimuth::fromDirectionYUp(az3.toDirectionYUp()).Azimuth, Catch::Matchers::WithinRel(az3.Azimuth + 2 * Pi));
    CHECK_THAT(ElevationAzimuth::fromDirectionYUp(az3.toDirectionYUp()).Elevation, Catch::Matchers::WithinRel(az3.Elevation));
}

TEST_CASE("Check directional mapping of elevation azimuth ZUp", "[ElevationAzimuth]")
{
    ElevationAzimuth az1{ 0, 0 };
    ElevationAzimuth az2{ 1, 1 };
    ElevationAzimuth az3{ 0, -Pi };

    CHECK_THAT(ElevationAzimuth::fromDirectionZUp(az1.toDirectionZUp()).Azimuth, Catch::Matchers::WithinRel(az1.Azimuth));
    CHECK_THAT(ElevationAzimuth::fromDirectionZUp(az1.toDirectionZUp()).Elevation, Catch::Matchers::WithinRel(az1.Elevation));
    CHECK_THAT(ElevationAzimuth::fromDirectionZUp(az2.toDirectionZUp()).Azimuth, Catch::Matchers::WithinRel(az2.Azimuth));
    CHECK_THAT(ElevationAzimuth::fromDirectionZUp(az2.toDirectionZUp()).Elevation, Catch::Matchers::WithinRel(az2.Elevation));
    CHECK_THAT(ElevationAzimuth::fromDirectionZUp(az3.toDirectionZUp()).Azimuth, Catch::Matchers::WithinRel(az3.Azimuth + 2 * Pi));
    CHECK_THAT(ElevationAzimuth::fromDirectionZUp(az3.toDirectionZUp()).Elevation, Catch::Matchers::WithinRel(az3.Elevation));
}

TEST_CASE("Nominal direction [90°, 0]", "[ElevationAzimuth]")
{
    ElevationAzimuth ea{ 90*Deg2Rad, 0 };
    const auto dir = ea.toDirectionZUp();

    constexpr float ParamEPS = 1e-4f;
    CHECK_THAT(dir.x(), Catch::Matchers::WithinAbs(0, ParamEPS));
    CHECK_THAT(dir.y(), Catch::Matchers::WithinAbs(0, ParamEPS));
    CHECK_THAT(dir.z(), Catch::Matchers::WithinAbs(1, ParamEPS));
}

TEST_CASE("Nominal direction [0, 0]", "[ElevationAzimuth]")
{
    ElevationAzimuth ea{ 0, 0 };
    const auto dir = ea.toDirectionZUp();

    constexpr float ParamEPS = 1e-4f;
    CHECK_THAT(dir.x(), Catch::Matchers::WithinAbs(0, ParamEPS));
    CHECK_THAT(dir.y(), Catch::Matchers::WithinAbs(-1, ParamEPS));
    CHECK_THAT(dir.z(), Catch::Matchers::WithinAbs(0, ParamEPS));
}

TEST_CASE("Nominal direction [0, 90°]", "[ElevationAzimuth]")
{
    ElevationAzimuth ea{ 0, 90*Deg2Rad };
    const auto dir = ea.toDirectionZUp();

    constexpr float ParamEPS = 1e-4f;
    CHECK_THAT(dir.x(), Catch::Matchers::WithinAbs(-1, ParamEPS));
    CHECK_THAT(dir.y(), Catch::Matchers::WithinAbs(0, ParamEPS));
    CHECK_THAT(dir.z(), Catch::Matchers::WithinAbs(0, ParamEPS));
}

TEST_CASE("Nominal direction [0, 180°]", "[ElevationAzimuth]")
{
    ElevationAzimuth ea{ 0, 180*Deg2Rad };
    const auto dir = ea.toDirectionZUp();

    constexpr float ParamEPS = 1e-4f;
    CHECK_THAT(dir.x(), Catch::Matchers::WithinAbs(0, ParamEPS));
    CHECK_THAT(dir.y(), Catch::Matchers::WithinAbs(1, ParamEPS));
    CHECK_THAT(dir.z(), Catch::Matchers::WithinAbs(0, ParamEPS));
}

TEST_CASE("Nominal direction [0, 270°]", "[ElevationAzimuth]")
{
    ElevationAzimuth ea{ 0, 270*Deg2Rad };
    const auto dir = ea.toDirectionZUp();

    constexpr float ParamEPS = 1e-4f;
    CHECK_THAT(dir.x(), Catch::Matchers::WithinAbs(1, ParamEPS));
    CHECK_THAT(dir.y(), Catch::Matchers::WithinAbs(0, ParamEPS));
    CHECK_THAT(dir.z(), Catch::Matchers::WithinAbs(0, ParamEPS));
}

TEST_CASE("Crosscheck with Radiance", "[ElevationAzimuth]")
{
    ElevationAzimuth az{ 12.5f * Deg2Rad, 39.1f * Deg2Rad };
    const auto dirRes     = az.toDirectionZUp();
    const Vector3f dirExp = Vector3f(-0.615494f, -0.757725f, 0.216842f);

    constexpr float ParamEPS = 1e-2f;
    CHECK_THAT(dirRes.x(), Catch::Matchers::WithinRel(dirExp.x(), ParamEPS));
    CHECK_THAT(dirRes.y(), Catch::Matchers::WithinRel(dirExp.y(), ParamEPS));
    CHECK_THAT(dirRes.z(), Catch::Matchers::WithinRel(dirExp.z(), ParamEPS));
}