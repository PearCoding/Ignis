#pragma once

#include "IG_Config.h"

namespace IG {
constexpr float SolarConstantE  = 1367;   // solar constant [W/m^2], defined in ISBN 3-519-03218-X
constexpr float SolarConstantL  = 127500; // solar constant [Lux]
constexpr float WhiteEfficiency = 179;    // Uniform white light efficiency over visible spectrum [lm/W] (defined in Radiance)
/// Sun solid angle seen from earth given in degrees;
constexpr float FltSunRadiusDegree = 0.533f;

inline float convertIrradianceToIlluminance(float a)
{
    // Very simplified model
    return a * SolarConstantL / SolarConstantE;
}

inline float convertIlluminanceToIrradiance(float a)
{
    // Very simplified model
    return a * SolarConstantE / SolarConstantL;
}

} // namespace IG