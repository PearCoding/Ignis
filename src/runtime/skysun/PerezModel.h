#pragma once

#include "IG_Config.h"

namespace IG {
class IG_LIB PerezModel {
public:
    inline float a() const { return mA; }
    inline float b() const { return mB; }
    inline float c() const { return mC; }
    inline float d() const { return mD; }
    inline float e() const { return mE; }

    /// @brief Compute perez value for given parameters
    /// @param cos_sun Cosine between sun and direction
    /// @param cos_theta Cosine between up and direction
    float eval(float cos_sun, float cos_theta) const;

    /// @brief Integrate hemi-sphere
    /// @param solar_zenith Solar zenith given radians
    float integrate(float solar_zenith) const;

    static float computeSkyBrightness(float diffuse_irradiance, float solar_zenith, int day_of_the_year);
    static float computeSkyClearness(float diffuse_irradiance, float direct_irradiance, float solar_zenith);

    static float computeDiffuseIrradiance(float sky_brightness, float solar_zenith, int day_of_the_year);
    static float computeDirectIrradiance(float sky_brightness, float sky_clearness, float solar_zenith, int day_of_the_year);

    static float computeDiffuseEfficacy(float sky_brightness, float sky_clearness, float solar_zenith);
    static float computeDirectEfficacy(float sky_brightness, float sky_clearness, float solar_zenith);

    /// Construct perez model directly with the given parameters
    static PerezModel fromParameters(float a, float b, float c, float d, float e);

    /// @brief Setup perez model with parameters determined by the Perez paper
    /// @param sky_brightness
    /// @param sky_clearness
    /// @param solar_zenith Solar zenith given radians
    static PerezModel fromSky(float sky_brightness, float sky_clearness, float solar_zenith);

    /// @brief Setup perez model using diffuse and direct irradiance values
    /// @param diffuse_irradiance Diffuse horizontal irradiance [W/m^2]
    /// @param direct_irradiance Direct normal irradiance [W/m^2]
    /// @param solar_zenith Solar zenith given radians
    /// @param day_of_the_year Day of the year between [0, 365]
    static PerezModel fromIrrad(float diffuse_irradiance, float direct_irradiance, float solar_zenith, int day_of_the_year);

    /// @brief Setup perez model using diffuse and direct illuminance values
    /// @param diffuse_irradiance Diffuse horizontal illuminance [Lux]
    /// @param direct_irradiance Direct normal illuminance [Lux]
    /// @param solar_zenith Solar zenith given radians
    /// @param day_of_the_year Day of the year between [0, 365]
    static PerezModel fromIllum(float diffuse_illuminance, float direct_illuminance, float solar_zenith, int day_of_the_year);

private:
    PerezModel(float a, float b, float c, float d, float e)
        : mA(a)
        , mB(b)
        , mC(c)
        , mD(d)
        , mE(e)
    {
    }

    float mA;
    float mB;
    float mC;
    float mD;
    float mE;
};
} // namespace IG