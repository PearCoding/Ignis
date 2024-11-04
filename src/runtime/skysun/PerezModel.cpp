#include "PerezModel.h"
#include "Illuminance.h"
#include "Logger.h"

namespace IG {

// Data based on 'Modeling Skylight Angular Luminance Distribution from Routine Irradiance Measurements' by R. Perez, R. Seals, and J. Michalsky (1993)
constexpr size_t BinCount         = 8;
static float Ranges[BinCount + 1] = { 1.000f, 1.065f, 1.230f, 1.500f, 1.950f, 2.800f, 4.500f, 6.200f, FltInf };

static float sA[BinCount * 4] = {
    1.3525f, -0.2576f, -0.2690f, -1.4366f,
    -1.2219f, -0.7730f, 1.4148f, 1.1016f,
    -1.1000f, -0.2515f, 0.8952f, 0.0156f,
    -0.5484f, -0.6654f, -0.2672f, 0.7117f,
    -0.6000f, -0.3566f, -2.5000f, 2.3250f,
    -1.0156f, -0.3670f, 1.0078f, 1.4051f,
    -1.0000f, 0.0211f, 0.5025f, -0.5119f,
    -1.0500f, 0.0289f, 0.4260f, 0.3590f
};

static float sB[BinCount * 4] = {
    -0.7670f, 0.0007f, 1.2734f, -0.1233f,
    -0.2054f, 0.0367f, -3.9128f, 0.9156f,
    0.2782f, -0.1812f, -4.5000f, 1.1766f,
    0.7234f, -0.6219f, -5.6812f, 2.6297f,
    0.2937f, 0.0496f, -5.6812f, 1.8415f,
    0.2875f, -0.5328f, -3.8500f, 3.3750f,
    -0.3000f, 0.1922f, 0.7023f, -1.6317f,
    -0.3250f, 0.1156f, 0.7781f, 0.0025f
};

static float sC[BinCount * 4] = {
    2.8000f, 0.6004f, 1.2375f, 1.0000f,
    6.9750f, 0.1774f, 6.4477f, -0.1239f,
    24.7219f, -13.0812f, -37.7000f, 34.8438f,
    33.3389f, -18.3000f, -62.2500f, 52.0781f,
    21.0000f, -4.7656f, -21.5906f, 7.2492f,
    14.0000f, -0.9999f, -7.1406f, 7.5469f,
    19.0000f, -5.0000f, 1.2438f, -1.9094f,
    31.0625f, -14.5000f, -46.1148f, 55.3750f
};

static float sD[BinCount * 4] = {
    1.8734f, 0.6297f, 0.9738f, 0.2809f,
    -1.5798f, -0.5081f, -1.7812f, 0.1080f,
    -5.0000f, 1.5218f, 3.9229f, -2.6204f,
    -3.5000f, 0.0016f, 1.1477f, 0.1062f,
    -3.5000f, -0.1554f, 1.4062f, 0.3988f,
    -3.4000f, -0.1078f, -1.0750f, 1.5702f,
    -4.0000f, 0.0250f, 0.3844f, 0.2656f,
    -7.2312f, 0.4050f, 13.3500f, 0.6234f
};

static float sE[BinCount * 4] = {
    0.0356f, -0.1246f, -0.5718f, 0.9938f,
    0.2624f, 0.0672f, -0.2190f, -0.4285f,
    -0.0156f, 0.1597f, 0.4199f, -0.5562f,
    0.4659f, -0.3296f, -0.0876f, -0.0329f,
    0.0032f, 0.0766f, -0.0656f, -0.1294f,
    -0.0672f, 0.4016f, 0.3017f, -0.4844f,
    1.0468f, -0.3788f, -2.4517f, 1.4656f,
    1.5000f, -0.6426f, 1.8564f, 0.5636f
};

static float sDiffuseA[BinCount] = { 97.24f, 107.22f, 104.97f, 102.39f, 100.71f, 106.42f, 141.88f, 152.23f };
static float sDiffuseB[BinCount] = { -0.46f, 1.15f, 2.96f, 5.59f, 5.94f, 3.83f, 1.90f, 0.35f };
static float sDiffuseC[BinCount] = { 12.00f, 0.59f, -5.53f, -13.95f, -22.75f, -36.15f, -53.24f, -45.27f };
static float sDiffuseD[BinCount] = { -8.91f, -3.95f, -8.77f, -13.90f, -23.74f, -28.83f, -14.03f, -7.98f };

static float sDirectA[BinCount] = { 57.20f, 98.99f, 109.83f, 110.34f, 106.36f, 107.19f, 105.75f, 101.18f };
static float sDirectB[BinCount] = { -4.55f, -3.46f, -4.90f, -5.84f, -3.97f, -1.25f, 0.77f, 1.58f };
static float sDirectC[BinCount] = { -2.98f, -1.21f, -1.71f, -1.99f, -1.75f, -1.51f, -1.26f, -1.10f };
static float sDirectD[BinCount] = { 117.12f, 12.38f, -8.81f, -4.56f, -6.16f, -26.73f, -34.44f, -8.29f };

float PerezModel::eval(float cos_sun, float cos_theta) const
{
    const float sun_a = std::acos(cos_sun); // Angle between sun and direction
    const float A     = 1 + mA * std::exp(mB / std::fmax(0.01f, cos_theta));
    const float B     = 1 + mC * std::exp(mD * sun_a) + mE * cos_sun * cos_sun;
    return A * B;
}

float PerezModel::integrate(const float solar_zenith) const
{
    const float cos_solar = std::cos(solar_zenith);
    const float sin_solar = std::sin(solar_zenith);

    const auto compute = [this, cos_solar, sin_solar](float cos_theta, float sin_theta, float cos_phi) {
        const float cos_sun = std::min(1.0f, cos_solar * cos_theta + sin_solar * sin_theta * cos_phi);
        return this->eval(cos_sun, cos_theta) * cos_theta;
    };

    // The first algorithm is not as precise as the second one, but due to backwards compatability, we are using the first one for now...
#if 1
    constexpr size_t BaseCount = 145;
    // Default integration base used in Radiance
    static float ThetaBase[BaseCount] = {
        84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84,
        84, 84, 84, 84, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
        72, 72, 72, 72, 72, 72, 72, 72, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
        60, 60, 60, 60, 60, 60, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
        48, 48, 48, 48, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 12, 12, 12, 12, 12, 12, 0
    };

    static float PhiBase[BaseCount] = {
        0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 132, 144, 156, 168, 180, 192, 204, 216, 228, 240, 252, 264,
        276, 288, 300, 312, 324, 336, 348, 0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 132, 144, 156, 168, 180,
        192, 204, 216, 228, 240, 252, 264, 276, 288, 300, 312, 324, 336, 348, 0, 15, 30, 45, 60, 75, 90, 105,
        120, 135, 150, 165, 180, 195, 210, 225, 240, 255, 270, 285, 300, 315, 330, 345, 0, 15, 30, 45, 60, 75,
        90, 105, 120, 135, 150, 165, 180, 195, 210, 225, 240, 255, 270, 285, 300, 315, 330, 345, 0, 20, 40, 60,
        80, 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300, 320, 340, 0, 30, 60, 90, 120, 150, 180, 210,
        240, 270, 300, 330, 0, 60, 120, 180, 240, 300, 0
    };

    float integrand = 0;
    for (size_t i = 0; i < BaseCount; ++i) {
        const float theta = ThetaBase[i] * Deg2Rad;
        const float ct    = std::cos(theta);
        const float st    = std::sin(theta);

        const float phi = PhiBase[i] * Deg2Rad;
        const float cp  = std::cos(phi);

        integrand += compute(ct, st, cp);
    }

    return 2 * Pi * integrand / BaseCount;
#else
    constexpr size_t ThetaCount = 32;
    constexpr size_t PhiCount   = 64;
    constexpr float MaxTheta    = 84 * Deg2Rad;
    constexpr float StepTheta   = MaxTheta / (float)(ThetaCount);

    float integrand = 0;
    for (size_t i = 0; i < ThetaCount; ++i) {
        const float theta = i == 0 ? 0.0f : (i + 0.5f) * StepTheta;
        const float ct    = std::cos(theta);
        const float st    = std::sin(theta);

        const size_t phi_count = i == 0 ? 1 : PhiCount;

        const float dth  = std::abs(std::cos((i + 1) * StepTheta) - std::cos(i * StepTheta));
        const float dphi = 2 * Pi / phi_count;
        for (size_t j = 0; j < phi_count; ++j) {
            const float phi = 2 * Pi * j / (float)phi_count;
            const float cp  = std::cos(phi);

            integrand += compute(ct, st, cp) * dphi * dth;
        }
    }

    return integrand;
#endif
}

float PerezModel::computeDiffuseEfficacy(float sky_brightness, float sky_clearness, float solar_zenith)
{
    // Get bin (this can be optimized by binary search)
    size_t bin = 0;
    for (; bin < BinCount; ++bin) {
        if (sky_clearness >= Ranges[bin] && sky_clearness < Ranges[bin + 1])
            break;
    }

    constexpr float Td          = 10.97353115f;                  // Dewpoint
    const float atm_preci_water = std::exp(0.07f * Td - 0.075f); // Atmospheric precipitable water content (=2)

    const float value = sDiffuseA[bin] + sDiffuseB[bin] * atm_preci_water + sDiffuseC[bin] * std::cos(solar_zenith) + sDiffuseD[bin] * std::log(sky_brightness);
    return value;
}

float PerezModel::computeDirectEfficacy(float sky_brightness, float sky_clearness, float solar_zenith)
{
    // Get bin (this can be optimized by binary search)
    size_t bin = 0;
    for (; bin < BinCount; ++bin) {
        if (sky_clearness >= Ranges[bin] && sky_clearness < Ranges[bin + 1])
            break;
    }

    constexpr float Td          = 10.97353115f;                  // Dewpoint
    const float atm_preci_water = std::exp(0.07f * Td - 0.075f); // Atmospheric precipitable water content (=2)

    const float value = sDirectA[bin] + sDirectB[bin] * atm_preci_water + sDirectC[bin] * std::exp(5.73f * solar_zenith - 5) + sDirectD[bin] * sky_brightness;
    return std::max(0.0f, value);
}

PerezModel PerezModel::fromParameters(float a, float b, float c, float d, float e)
{
    IG_LOG(L_DEBUG) << "Perez: " << a << " " << b << " " << c << " " << d << " " << e << std::endl;
    return PerezModel(a, b, c, d, e);
}

PerezModel PerezModel::fromSky(float sky_brightness, float sky_clearness, float solar_zenith)
{
    // Validate parametrization (limits defined by J. Wienold - Radiance)
    constexpr float SkyClearnessMin = 1.0f;
    constexpr float SkyClearnessMax = 12.01f;
    if (sky_clearness < SkyClearnessMin)
        sky_clearness = SkyClearnessMin;
    if (sky_clearness > SkyClearnessMax)
        sky_clearness = SkyClearnessMax - 0.001f;

    constexpr float SkyBrightnessMin = 0.01f;
    constexpr float SkyBrightnessMax = 0.6f;
    if (sky_brightness < SkyBrightnessMin)
        sky_brightness = SkyBrightnessMin;
    if (sky_brightness > SkyBrightnessMax)
        sky_brightness = SkyBrightnessMax;

    // Correction factor in the Perez model
    if ((sky_clearness > 1.065f) && (sky_clearness < 2.8f)) {
        if (sky_brightness < 0.2f)
            sky_brightness = 0.2f;
    }

    const auto compute         = [=](const float* x) { return x[0] + x[1] * solar_zenith + sky_brightness * (x[2] + x[3] * solar_zenith); };
    const auto computeSpecialC = [=]() { return std::exp(std::pow(sky_brightness * (sC[0] + sC[1] * solar_zenith), sC[2])) - sC[3]; };
    const auto computeSpecialD = [=]() { return -std::exp(sky_brightness * (sD[0] + sD[1] * solar_zenith)) + sD[2] + sky_brightness * sD[3]; };

    // Get bin (this can be optimized by binary search)
    size_t bin = 0;
    for (; bin < BinCount; ++bin) {
        if (sky_clearness >= Ranges[bin] && sky_clearness < Ranges[bin + 1])
            break;
    }

    // Compute parameters
    const float a = compute(&sA[bin * 4]);
    const float b = compute(&sB[bin * 4]);
    const float c = bin > 0 ? compute(&sC[bin * 4]) : computeSpecialC();
    const float d = bin > 0 ? compute(&sD[bin * 4]) : computeSpecialD();
    const float e = compute(&sE[bin * 4]);

    return PerezModel::fromParameters(a, b, c, d, e);
}

static inline float computeAirMass(float solar_zenith)
{
    float solar_zenith_deg = std::min(Rad2Deg * solar_zenith, 90.0f);
    return 1 / (std::cos(Deg2Rad * solar_zenith_deg) + 0.15f * std::exp(std::log(93.885f - solar_zenith_deg) * -1.253f));
}

static inline float computeEccentricity(float day_of_the_year)
{
    float day_angle = 2 * Pi * std::min(std::max(day_of_the_year / 365.0f, 0.0f), 1.0f);
    return 1.00011f + 0.034221f * std::cos(day_angle) + 0.00128f * std::sin(day_angle) + 0.000719f * std::cos(2 * day_angle) + 0.000077f * std::sin(2 * day_angle);
}

float PerezModel::computeSkyBrightness(float diff_irrad, float solar_zenith, float day_of_the_year)
{
    return diff_irrad * computeAirMass(solar_zenith) / (SolarConstantE * computeEccentricity(day_of_the_year));
}

float PerezModel::computeSkyClearness(float diff_irrad, float direct_irrad, float solar_zenith)
{
    const float A = 1.041f * solar_zenith * solar_zenith * solar_zenith;
    return ((diff_irrad + direct_irrad) / diff_irrad + A) / (1 + A);
}

float PerezModel::computeDiffuseIrradiance(float sky_brightness, float solar_zenith, float day_of_the_year)
{
    return sky_brightness * SolarConstantE * computeEccentricity(day_of_the_year) / computeAirMass(solar_zenith);
}

float PerezModel::computeDirectIrradiance(float sky_brightness, float sky_clearness, float solar_zenith, float day_of_the_year)
{
    const float diff_irrad = computeDiffuseIrradiance(sky_brightness, solar_zenith, day_of_the_year);
    const float A          = 1.041f * solar_zenith * solar_zenith * solar_zenith;

    return (sky_clearness * (1 + A) - A) * diff_irrad - diff_irrad;
}

PerezModel PerezModel::fromIrrad(float diffuse_irradiance, float direct_irradiance, float solar_zenith, float day_of_the_year)
{
    if (diffuse_irradiance < 0)
        diffuse_irradiance = 0;
    if (direct_irradiance < 0)
        direct_irradiance = 0;

    // TODO: Check against SolarConstantE

    return fromSky(computeSkyBrightness(diffuse_irradiance, solar_zenith, day_of_the_year),
                   computeSkyClearness(diffuse_irradiance, direct_irradiance, solar_zenith),
                   solar_zenith);
}

PerezModel PerezModel::fromIllum(float diffuse_illuminance, float direct_illuminance, float solar_zenith, float day_of_the_year)
{
    if (diffuse_illuminance < 0)
        diffuse_illuminance = 0;
    if (direct_illuminance < 0)
        direct_illuminance = 0;

    // TODO: Check against SolarConstantL

    return fromIrrad(convertIlluminanceToIrradiance(diffuse_illuminance),
                     convertIlluminanceToIrradiance(direct_illuminance),
                     solar_zenith, day_of_the_year);
}

} // namespace IG