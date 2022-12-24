#include "skysun/SunLocation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace IG;
TEST_CASE("Check output", "[Sun]")
{
    TimePoint timepoint  = TimePoint(2022, 11, 18, 13, 0, 0);
    MapLocation location = MapLocation(-6.9965744f, 49.235422f, -1);

    const auto ea = computeSunEA(timepoint, location);

    ElevationAzimuth exp{ 20.86f * Deg2Rad, 10.81f * Deg2Rad /* West of South */ };

    constexpr float ParamEPS = 1e-2f;
    CHECK_THAT(ea.Elevation, Catch::Matchers::WithinRel(exp.Elevation, ParamEPS));
    CHECK_THAT(ea.Azimuth, Catch::Matchers::WithinRel(exp.Azimuth, ParamEPS));
}

TEST_CASE("Orientation crosscheck with Radiance", "[Sun]")
{
    TimePoint timepoint  = TimePoint(2022, 11, 18, 13, 0, 0);
    MapLocation location = MapLocation(-6.9965744f, 49.235422f, -1);

    const auto ea  = computeSunEA(timepoint, location);
    const auto dir = ea.toDirectionZUp();

    ElevationAzimuth expEA{ 20.8f * Deg2Rad, 10.8f * Deg2Rad };
    Vector3f expSunDir(-0.175382f, -0.918072f, 0.355506f);

    constexpr float ParamEPS = 1e-3f;
    CHECK_THAT(ea.Elevation, Catch::Matchers::WithinAbs(expEA.Elevation, ParamEPS));
    CHECK_THAT(ea.Azimuth, Catch::Matchers::WithinAbs(expEA.Azimuth, ParamEPS));

    CHECK_THAT(dir.x(), Catch::Matchers::WithinRel(expSunDir.x(), ParamEPS));
    CHECK_THAT(dir.y(), Catch::Matchers::WithinRel(expSunDir.y(), ParamEPS));
    CHECK_THAT(dir.z(), Catch::Matchers::WithinRel(expSunDir.z(), ParamEPS));
}