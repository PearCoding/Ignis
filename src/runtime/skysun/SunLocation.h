#pragma once

#include "ElevationAzimuth.h"

namespace IG {

// Default is 2020.05.06 12:00:00 (midday)
struct IG_LIB TimePoint {
    int Year      = 2020;
    int Month     = 5;
    int Day       = 6;
    int Hour      = 12;
    int Minute    = 0;
    float Seconds = 0.0f;

    TimePoint() = default;
    TimePoint(int year, int month, int day, int hour = 0, int minute = 0, float seconds = 0)
        : Year(year)
        , Month(month)
        , Day(day)
        , Hour(hour)
        , Minute(minute)
        , Seconds(seconds)
    {
    }

    /// Return the day of the year since the 1st of January
    int dayOfTheYear() const;
    /// Return the day of the year since the 1st of January 4713 BC
    float julianDate(float timezone) const;

    /// @brief Calcuate the decimal hour
    /// @param timezone 
    /// @return 
    float decimalHours(float timezone) const;
};

// Default is Saarbrücken, Elevation: 52.87 Azimuth: 323.271 (west of south)
struct MapLocation {
    float Longitude = -6.9965744f; // Degrees west
    float Latitude  = 49.235422f;  // Degrees north
    float Timezone  = -2;          // Offset to UTC

    MapLocation() = default;
    MapLocation(float longitude, float latitude, float timezone = 0)
        : Longitude(longitude)
        , Latitude(latitude)
        , Timezone(timezone)
    {
    }
};

/// Will return elevation and azimuth (west of south)
[[nodiscard]] IG_LIB ElevationAzimuth computeSunEA(const TimePoint& timepoint, const MapLocation& location);
} // namespace IG