#include "skysun/Illuminance.h"
#include "skysun/PerezModel.h"
#include "skysun/SunLocation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace IG;
TEST_CASE("Crosscheck with Radiance gendaylit", "[Perez]")
{
    const float diffirrad     = 57.029998779296875f;
    const float dirirrad      = 0.38999998569488525f;
    const Vector3f sunDir     = Vector3f(-0.615494f, -0.757725f, 0.216842f);
    const float solar_zenith  = std::acos(sunDir.z()); // Note: Ignis uses +Y as Up vector by default not +Z like here
    const int day_of_the_year = TimePoint(2022, 11, 18, 13).dayOfTheYear();
    PerezModel model          = PerezModel::fromIrrad(diffirrad, dirirrad, solar_zenith, day_of_the_year);
    const float integrand     = model.integrate(solar_zenith);
    const float diffnorm      = diffirrad / integrand;
    // const float diffillumnorm = convertIrradianceToIlluminance(diffirrad) / (integrand * WhiteEfficiency); // TODO: Not that simple
    const float epsilon       = PerezModel::computeSkyClearness(diffirrad, dirirrad, solar_zenith);
    const float delta         = PerezModel::computeSkyBrightness(diffirrad, solar_zenith, day_of_the_year);

    // gendaylit 11 18 13 -y 2022 -a +49.235422 -o -6.9965744 -m 15 -s -W 0.38999998569488525 57.029998779296875 -O 1
    // -> 1.064e+01 4.560e+00 0.597123 -0.562370 0.828195 -0.625727 0.009207 -0.615494 -0.757725 0.216842

    constexpr float ParamEPS = 1e-4f;
    CHECK_THAT(model.a(), Catch::Matchers::WithinRel(0.597123f, ParamEPS));
    CHECK_THAT(model.b(), Catch::Matchers::WithinRel(-0.562370f, ParamEPS));
    CHECK_THAT(model.c(), Catch::Matchers::WithinRel(0.828195f, ParamEPS));
    CHECK_THAT(model.d(), Catch::Matchers::WithinRel(-0.625727f, ParamEPS));
    CHECK_THAT(model.e(), Catch::Matchers::WithinRel(0.009207f, ParamEPS));

    constexpr float SkyEPS = 1e-3f;
    CHECK_THAT(solar_zenith, Catch::Matchers::WithinRel(77.5f * Deg2Rad, SkyEPS));
    CHECK_THAT(epsilon, Catch::Matchers::WithinRel(1.0019f, SkyEPS));
    CHECK_THAT(delta, Catch::Matchers::WithinRel(0.1841f, SkyEPS));

    constexpr float NormEPS = 1e-2f;
    CHECK_THAT(diffnorm, Catch::Matchers::WithinRel(10.64f, NormEPS));
    // CHECK_THAT(diffillumnorm, Catch::Matchers::WithinRel(6.774f, NormEPS));
}