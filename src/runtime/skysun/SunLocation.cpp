#include "SunLocation.h"

#include <time.h>

namespace IG {

int TimePoint::dayOfTheYear() const
{
    tm date      = {};
    date.tm_sec  = (int)Seconds;
    date.tm_min  = Minute;
    date.tm_hour = Hour;
    date.tm_year = Year - 1900;
    date.tm_mon  = Month - 1;
    date.tm_mday = Day;
    mktime(&date); // Will set `tm_yday` and `tm_wday`
    return date.tm_yday;
}

float TimePoint::julianDate(float timezone) const
{
    float decHours = decimalHours(timezone);

    // Calculate current Julian Day
    int liAux1 = (Month - 14) / 12;
    int liAux2 = (1461 * (Year + 4800 + liAux1)) / 4
                 + (367 * (Month - 2 - 12 * liAux1)) / 12
                 - (3 * ((Year + 4900 + liAux1) / 100)) / 4
                 + Day - 32075;
    return (float)liAux2 - 0.5f + decHours / 24.0f;
}

float TimePoint::decimalHours(float timezone) const
{
    return Hour + timezone + (Minute + Seconds / 60.0f) / 60.0f;
}

TimePoint TimePoint::nowUTC()
{
    const time_t time = std::time(0);
    std::tm* tm       = std::gmtime(&time);

    TimePoint tp;
    tp.Year    = tm->tm_year + 1900;
    tp.Month   = tm->tm_mon + 1;
    tp.Day     = tm->tm_mday;
    tp.Hour    = tm->tm_hour;
    tp.Minute  = tm->tm_min;
    tp.Seconds = tm->tm_sec;
    return tp;
}

/* Based on "Computing the Solar Vector" by Manuel Blanco-Muriel,
 * Diego C. Alarcon-Padilla, Teodoro Lopez-Moratalla, and Martin Lara-Coira,
 * in "Solar energy", vol 27, number 5, 2001 by Pergamon Press.
 */
ElevationAzimuth computeSunEA(const TimePoint& timepoint, const MapLocation& location)
{
    constexpr double EARTH_MEAN_RADIUS = 6371.01;   // In km
    constexpr double ASTRONOMICAL_UNIT = 149597890; // In km

    /* Calculate difference in days between the current Julian Day
       and JD 2451545.0, which is noon 1 January 2000 Universal Time */
    const double elapsedJulianDays = timepoint.julianDate(location.Timezone) - 2451545.0f;
    const double decHours          = timepoint.decimalHours(location.Timezone);

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
        double dY                   = std::cos(eclipticObliquity) * sinEclipticLongitude;
        double dX                   = std::cos(eclipticLongitude);
        rightAscension              = std::atan2(dY, dX);
        if (rightAscension < 0)
            rightAscension += 2 * Pi;
        declination = std::asin(std::sin(eclipticObliquity) * sinEclipticLongitude);
    }

    // Calculate local zenith and azimuth angles
    double zenith = 0, azimuth = 0;
    {
        double greenwichMeanSiderealTime = 6.6974243242 + 0.0657098283 * elapsedJulianDays + decHours;

        double localMeanSiderealTime = Deg2Rad * ((float)((greenwichMeanSiderealTime * 15 - location.Longitude)));

        double latitudeInRadians = Deg2Rad * location.Latitude;
        double cosLatitude       = std::cos(latitudeInRadians);
        double sinLatitude       = std::sin(latitudeInRadians);

        double hourAngle    = localMeanSiderealTime - rightAscension;
        double cosHourAngle = std::cos(hourAngle);

        zenith = std::acos(cosLatitude * cosHourAngle * std::cos(declination) + std::sin(declination) * sinLatitude);

        double dY = -std::sin(hourAngle);
        double dX = std::tan(declination) * cosLatitude - sinLatitude * cosHourAngle;

        azimuth = std::atan2(dY, dX);
        if (azimuth < 0)
            azimuth += 2 * Pi;

        // Parallax Correction
        zenith += (EARTH_MEAN_RADIUS / ASTRONOMICAL_UNIT) * std::sin(zenith);
    }

    return ElevationAzimuth{ Pi2 - (float)zenith, std::fmod((float)azimuth + Pi, 2 * Pi) /* The original azimuth is given in east of north, we want west of south */ };
}
} // namespace IG