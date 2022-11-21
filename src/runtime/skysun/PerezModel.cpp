#include "PerezModel.h"
#include "Logger.h"

namespace IG {
constexpr float SolarConstantE = 1367;   // solar constant [W/m^2]
constexpr float SolarConstantL = 127500; // solar constant [Lux]

// Data based on 'Modeling Skylight Angular Luminance Distribution from Routine Irradiance Measurements' by R. Perez, R. Seals, and J. Michalsky (1993)
constexpr size_t BinCount         = 8;
static float Ranges[BinCount + 1] = { 1.000f, 1.065f, 1.230f, 1.500f, 1.950f, 2.800f, 4.500f, 6.200f, FltInf };

static float A[BinCount * 4] = {
    1.3525f, -0.2576f, -0.2690f, -1.4366f,
    -1.2219f, -0.7730f, 1.4148f, 1.1016f,
    -1.1000f, -0.2515f, 0.8952f, 0.0156f,
    -0.5484f, -0.6654f, -0.2672f, 0.7117f,
    -0.6000f, -0.3566f, -2.5000f, 2.3250f,
    -1.0156f, -0.3670f, 1.0078f, 1.4051f,
    -1.0000f, 0.0211f, 0.5025f, -0.5119f,
    -1.0500f, 0.0289f, 0.4260f, 0.3590f
};

static float B[BinCount * 4] = {
    -0.7670f, 0.0007f, 1.2734f, -0.1233f,
    -0.2054f, 0.0367f, -3.9128f, 0.9156f,
    0.2782f, -0.1812f, -4.5000f, 1.1766f,
    0.7234f, -0.6219f, -5.6812f, 2.6297f,
    0.2937f, 0.0496f, -5.6812f, 1.8415f,
    0.2875f, -0.5328f, -3.8500f, 3.3750f,
    -0.3000f, 0.1922f, 0.7023f, -1.6317f,
    -0.3250f, 0.1156f, 0.7781f, 0.0025f
};

static float C[BinCount * 4] = {
    2.8000f, 0.6004f, 1.2375f, 1.0000f,
    6.9750f, 0.1774f, 6.4477f, -0.1239f,
    24.7219f, -13.0812f, -37.7000f, 34.8438f,
    33.3389f, -18.3000f, -62.2500f, 52.0781f,
    21.0000f, -4.7656f, -21.5906f, 7.2492f,
    14.0000f, -0.9999f, -7.1406f, 7.5469f,
    19.0000f, -5.0000f, 1.2438f, -1.9094f,
    31.0625f, -14.5000f, -46.1148f, 55.3750f
};

static float D[BinCount * 4] = {
    1.8734f, 0.6297f, 0.9738f, 0.2809f,
    -1.5798f, -0.5081f, -1.7812f, 0.1080f,
    -5.0000f, 1.5218f, 3.9229f, -2.6204f,
    -3.5000f, 0.0016f, 1.1477f, 0.1062f,
    -3.5000f, -0.1554f, 1.4062f, 0.3988f,
    -3.4000f, -0.1078f, -1.0750f, 1.5702f,
    -4.0000f, 0.0250f, 0.3844f, 0.2656f,
    -7.2312f, 0.4050f, 13.3500f, 0.6234f
};

static float E[BinCount * 4] = {
    0.0356f, -0.1246f, -0.5718f, 0.9938f,
    0.2624f, 0.0672f, -0.2190f, -0.4285f,
    -0.0156f, 0.1597f, 0.4199f, -0.5562f,
    0.4659f, -0.3296f, -0.0876f, -0.0329f,
    0.0032f, 0.0766f, -0.0656f, -0.1294f,
    -0.0672f, 0.4016f, 0.3017f, -0.4844f,
    1.0468f, -0.3788f, -2.4517f, 1.4656f,
    1.5000f, -0.6426f, 1.8564f, 0.5636f
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