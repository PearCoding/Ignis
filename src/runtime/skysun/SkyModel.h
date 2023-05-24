#pragma once

#include "Color.h"
#include "ElevationAzimuth.h"
#include "SkySunConfig.h"

namespace IG {
struct ElevationAzimuth;
class Serializer;

class SkyModel {
public:
    SkyModel(const RGB& ground_albedo, const ElevationAzimuth& sunEA, float turbidity = 3.0f, size_t resAzimuth = RES_AZ, size_t resElevation = RES_EL);

    [[nodiscard]] inline size_t azimuthCount() const { return mAzimuthCount; }
    [[nodiscard]] inline size_t elevationCount() const { return mElevationCount; }

    [[nodiscard]] inline RGB radiance(const ElevationAzimuth& ea) const
    {
        int az_in = std::max(0, std::min<int>(static_cast<int>(mAzimuthCount) - 1, static_cast<int>(ea.Azimuth / AZIMUTH_RANGE * static_cast<float>(mAzimuthCount))));
        int el_in = std::max(0, std::min<int>(static_cast<int>(mElevationCount) - 1, static_cast<int>(ea.Elevation / ELEVATION_RANGE * static_cast<float>(mElevationCount))));

        return RGB{
            mData[el_in * mAzimuthCount * 4 + az_in * 4 + 0],
            mData[el_in * mAzimuthCount * 4 + az_in * 4 + 1],
            mData[el_in * mAzimuthCount * 4 + az_in * 4 + 2]
        };
    }

    [[nodiscard]] RGB computeTotal() const; 

    bool save(const Path& path) const;

private:
    std::vector<float> mData;

    size_t mAzimuthCount;
    size_t mElevationCount;
};
} // namespace IG