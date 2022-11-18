#include "PerezModel.h"
#include "Logger.h"

namespace IG {
constexpr float SolarConstantE = 1367;   // solar constant [W/m^2]
constexpr float SolarConstantL = 127500; // solar constant [Lux]

// Data based on 'Modeling Skylight Angular Luminance Distribution from Routine Irradiance Measurements' by R. Perez, R. Seals, and J. Michalsky (1993)
constexpr size_t BinCount         = 8;
static float Ranges[BinCount + 1] = { 1.000, 1.065, 1.230, 1.500, 1.950, 2.800, 4.500, 6.200, FltInf };

static float A[BinCount * 4] = {
    1.3525, -0.2576, -0.2690, -1.4366,
    -1.2219, -0.7730, 1.4148, 1.1016,
    -1.1000, -0.2515, 0.8952, 0.0156,
    -0.5484, -0.6654, -0.2672, 0.7117,
    -0.6000, -0.3566, -2.5000, 2.3250,
    -1.0156, -0.3670, 1.0078, 1.4051,
    -1.0000, 0.0211, 0.5025, -0.5119,
    -1.0500, 0.0289, 0.4260, 0.3590
};

static float B[BinCount * 4] = {
    -0.7670, 0.0007, 1.2734, -0.1233,
    -0.2054, 0.0367, -3.9128, 0.9156,
    0.2782, -0.1812, -4.5000, 1.1766,
    0.7234, -0.6219, -5.6812, 2.6297,
    0.2937, 0.0496, -5.6812, 1.8415,
    0.2875, -0.5328, -3.8500, 3.3750,
    -0.3000, 0.1922, 0.7023, -1.6317,
    -0.3250, 0.1156, 0.7781, 0.0025
};

static float C[BinCount * 4] = {
    2.8000, 0.6004, 1.2375, 1.0000,
    6.9750, 0.1774, 6.4477, -0.1239,
    24.7219, -13.0812, -37.7000, 34.8438,
    33.3389, -18.3000, -62.2500, 52.0781,
    21.0000, -4.7656, -21.5906, 7.2492,
    14.0000, -0.9999, -7.1406, 7.5469,
    19.0000, -5.0000, 1.2438, -1.9094,
    31.0625, -14.5000, -46.1148, 55.3750
};

static float D[BinCount * 4] = {
    1.8734, 0.6297, 0.9738, 0.2809,
    -1.5798, -0.5081, -1.7812, 0.1080,
    -5.0000, 1.5218, 3.9229, -2.6204,
    -3.5000, 0.0016, 1.1477, 0.1062,
    -3.5000, -0.1554, 1.4062, 0.3988,
    -3.4000, -0.1078, -1.0750, 1.5702,
    -4.0000, 0.0250, 0.3844, 0.2656,
    -7.2312, 0.4050, 13.3500, 0.6234
};

static float E[BinCount * 4] = {
    0.0356, -0.1246, -0.5718, 0.9938,
    0.2624, 0.0672, -0.2190, -0.4285,
    -0.0156, 0.1597, 0.4199, -0.5562,
    0.4659, -0.3296, -0.0876, -0.0329,
    0.0032, 0.0766, -0.0656, -0.1294,
    -0.0672, 0.4016, 0.3017, -0.4844,
    1.0468, -0.3788, -2.4517, 1.4656,
    1.5000, -0.6426, 1.8564, 0.5636
};

PerezModel PerezModel::fromParameter(float a, float b, float c, float d, float e)
{
    IG_LOG(L_DEBUG) << "Perez: " << a << " " << b << " " << c << " " << d << " " << e << std::endl;
    return PerezModel(a, b, c, d, e);
}

PerezModel PerezModel::fromSky(float sky_brightness, float sky_clearness, float solar_zenith)
{
    // Correction factor in the Perez model
    if ((sky_clearness > 1.065f) && (sky_clearness < 2.8f)) {
        if (sky_brightness < 0.2f)
            sky_brightness = 0.2f;
    }

    sky_brightness = std::min(std::max(sky_brightness, 0.01f), 0.6f);

    const auto compute         = [=](const float* x) { return x[0] + x[1] * solar_zenith + sky_brightness * (x[2] + x[3] * solar_zenith); };
    const auto computeSpecialC = [=]() { return std::exp(std::pow(sky_brightness * (C[0] + C[1] * solar_zenith), C[2])) - C[3]; };
    const auto computeSpecialD = [=]() { return -std::exp(sky_brightness * (D[0] + D[1] * solar_zenith)) + D[2] + sky_brightness * D[3]; };

    // Get bin (this can be optimized by binary search)
    size_t bin = 0;
    for (; bin < BinCount; ++bin) {
        if (sky_clearness >= Ranges[bin] && sky_clearness < Ranges[bin + 1])
            break;
    }

    // Compute parameters
    const float a = compute(&A[bin * 4]);
    const float b = compute(&B[bin * 4]);
    const float c = bin > 0 ? compute(&C[bin * 4]) : computeSpecialC();
    const float d = bin > 0 ? compute(&D[bin * 4]) : computeSpecialD();
    const float e = compute(&E[bin * 4]);

    return PerezModel::fromParameter(a, b, c, d, e);
}

static inline float computeAirMass(float solar_zenith)
{
    float solar_zenith_deg = std::min(Rad2Deg * solar_zenith, 90.0f);
    return 1 / (std::cos(Deg2Rad * solar_zenith_deg) + 0.15f * std::exp(std::log(93.885f - solar_zenith_deg) * -1.253f));
}

static inline float computeEccentricity(int day_of_the_year)
{
    float day_angle = 2 * Pi * std::min(std::max(day_of_the_year / 365.0f, 0.0f), 1.0f);
    return 1.00011f + 0.034221f * std::cos(day_angle) + 0.00128f * std::sin(day_angle) + 0.000719f * std::cos(2 * day_angle) + 0.000077f * std::sin(2 * day_angle);
}

static inline float computeSkyBrightness(float diff_irrad, float solar_zenith, int day_of_the_year)
{
    return diff_irrad * computeAirMass(solar_zenith) / (SolarConstantE * computeEccentricity(day_of_the_year));
}

static inline float computeSkyClearness(float diff_irrad, float diret_irrad, float solar_zenith)
{
    return ((diff_irrad + diret_irrad) / diff_irrad + 1.041f * solar_zenith * solar_zenith * solar_zenith) / (1 + 1.041f * solar_zenith * solar_zenith * solar_zenith);
}

PerezModel PerezModel::fromIrrad(float diffuse_irradiance, float direct_irradiance, float solar_zenith, int day_of_the_year)
{
    return fromSky(computeSkyBrightness(diffuse_irradiance, solar_zenith, day_of_the_year),
                   computeSkyClearness(diffuse_irradiance, direct_irradiance, solar_zenith),
                   solar_zenith);
}

PerezModel PerezModel::fromIllum(float diffuse_illuminance, float direct_illuminance, float solar_zenith, int day_of_the_year)
{
    return fromIrrad(diffuse_illuminance * SolarConstantE / SolarConstantL,
                     direct_illuminance * SolarConstantE / SolarConstantL,
                     solar_zenith, day_of_the_year);
}

} // namespace IG