#include "skysun/ElevationAzimuth.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace IG;
TEST_CASE("Check directional mapping of elevation azimuth", "[ElevationAzimuth]")
{
    ElevationAzimuth az1{ 0, 0 };
    ElevationAzimuth az2{ 1, 1 };
    ElevationAzimuth az3{ 0, -Pi };

    CHECK_THAT(ElevationAzimuth::fromDirection(az1.toDirection()).Azimuth, Catch::Matchers::WithinRel(az1.Azimuth));
    CHECK_THAT(ElevationAzimuth::fromDirection(az1.toDirection()).Elevation, Catch::Matchers::WithinRel(az1.Elevation));
    CHECK_THAT(ElevationAzimuth::fromDirection(az2.toDirection()).Azimuth, Catch::Matchers::WithinRel(az2.Azimuth));
    CHECK_THAT(ElevationAzimuth::fromDirection(az2.toDirection()).Elevation, Catch::Matchers::WithinRel(az2.Elevation));
    CHECK_THAT(ElevationAzimuth::fromDirection(az3.toDirection()).Azimuth, Catch::Matchers::WithinRel(az3.Azimuth + 2 * Pi));
    CHECK_THAT(ElevationAzimuth::fromDirection(az3.toDirection()).Elevation, Catch::Matchers::WithinRel(az3.Elevation));
}