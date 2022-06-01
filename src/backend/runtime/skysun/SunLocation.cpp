#include "SunLocation.h"

namespace IG {

/* Based on "Computing the Solar Vector" by Manuel Blanco-Muriel,
 * Diego C. Alarcon-Padilla, Teodoro Lopez-Moratalla, and Martin Lara-Coira,
 * in "Solar energy", vol 27, number 5, 2001 by Pergamon Press.
 */
ElevationAzimuth computeSunEA(const TimePoint& timepoint, const MapLocation& location)
{
    constexpr double EARTH_MEAN_RADIUS = 6371.01;   // In km
    constexpr double ASTRONOMICAL_UNIT = 149597890; // In km

    // Auxiliary variables
    double dY = 0;
    double dX = 0;

    /* Calculate difference in days between the current Julian Day
       and JD 2451545.0, which is noon 1 January 2000 Universal Time */
    double elapsedJulianDays = 0, decHours = 0;
    {
        // Calculate time of the day in UT decimal hours
        decHours = timepoint.Hour - location.Timezone + (timepoint.Minute + timepoint.Seconds / 60.0) / 60.0;

        // Calculate current Julian Day
        int liAux1 = (timepoint.Month - 14) / 12;
        int liAux2 = (1461 * (timepoint.Year + 4800 + liAux1)) / 4
                     + (367 * (timepoint.Month - 2 - 12 * liAux1)) / 12
                     - (3 * ((timepoint.Year + 4900 + liAux1) / 100)) / 4
                     + timepoint.Day - 32075;
        double dJulianDate = (double)liAux2 - 0.5 + decHours / 24.0;

        // Calculate difference between current Julian Day and JD 2451545.0
        elapsedJulianDays = dJulianDate - 2451545.0;
    }

    /* Calculate ecliptic coordinates (ecliptic longitude and obliquity of the
       ecliptic in radians but without limiting the angle to be less than 2*Pi
       (i.e., the result may be greater than 2*Pi) */
    double eclipticLongitude = 0, eclipticObliquity = 0;
    {
        double omega         = 2.1429 - 0.0010394594 * elapsedJulianDays;
        double meanLongitude = 4.8950630 + 0.017202791698 * elapsedJulianDays; // Radians
        double anomaly       = 6.2400600 + 0.0172019699 * elapsedJulianDays;

        eclipticLongitude = meanLongitude + 0.03341607 * std::sin(anomaly)
                            + 0.00034894 * std::sin(2 * anomaly) - 0.0001134
                            - 0.0000203 * std::sin(omega);

        eclipticObliquity = 0.4090928 - 6.2140e-9 * elapsedJulianDays
                            + 0.0000396 * std::cos(omega);
    }

    /* Calculate celestial coordinates ( right ascension and declination ) in radians
       but without limiting the angle to be less than 2*Pi (i.e., the result may be
       greater than 2*Pi) */
    double rightAscension = 0, declination = 0;
    {
        double sinEclipticLongitude = std::sin(eclipticLongitude);
        dY                          = std::cos(eclipticObliquity) * sinEclipticLongitude;
        dX                          = std::cos(eclipticLongitude);
        rightAscension              = std::atan2(dY, dX);
        if (rightAscension < 0.0)
            rightAscension += 2 * Pi;
        declination = std::asin(std::sin(eclipticObliquity) * sinEclipticLongitude);
    }

    // Calculate local zenith and azimuth angles
    double zenith = 0, azimuth = 0;
    {
        double greenwichMeanSiderealTime = 6.6974243242
                                           + 0.0657098283 * elapsedJulianDays + decHours;

        double localMeanSiderealTime = Deg2Rad * ((float)((greenwichMeanSiderealTime * 15 + location.Longitude)));

        double latitudeInRadians = Deg2Rad * location.Latitude;
        double cosLatitude       = std::cos(latitudeInRadians);
        double sinLatitude       = std::sin(latitudeInRadians);

        double hourAngle    = localMeanSiderealTime - rightAscension;
        double cosHourAngle = std::cos(hourAngle);

        zenith = std::acos(cosLatitude * cosHourAngle
                               * std::cos(declination)
                           + std::sin(declination) * sinLatitude);

        dY = -std::sin(hourAngle);
        dX = std::tan(declination) * cosLatitude - sinLatitude * cosHourAngle;

        azimuth = std::atan2(dY, dX);
        if (azimuth < 0.0)
            azimuth += 2 * Pi;

        // Parallax Correction
        zenith += (EARTH_MEAN_RADIUS / ASTRONOMICAL_UNIT) * std::sin(zenith);
    }

    return ElevationAzimuth{ Pi2 - (float)zenith, 2 * Pi - (float)azimuth /* The original azimuth is given in east of north, we want west of south */ };
}
} // namespace IG