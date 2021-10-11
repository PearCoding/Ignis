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

	inline size_t azimuthCount() const { return mAzimuthCount; }
	inline size_t elevationCount() const { return mElevationCount; }

	inline RGB radiance(const ElevationAzimuth& ea) const
	{
		int az_in = std::max(0, std::min<int>(mAzimuthCount - 1, int(ea.Azimuth / AZIMUTH_RANGE * mAzimuthCount)));
		int el_in = std::max(0, std::min<int>(mElevationCount - 1, int(ea.Elevation / ELEVATION_RANGE * mElevationCount)));

		return RGB{
			mData[el_in * mAzimuthCount * AR_COLOR_BANDS + az_in * AR_COLOR_BANDS + 0],
			mData[el_in * mAzimuthCount * AR_COLOR_BANDS + az_in * AR_COLOR_BANDS + 1],
			mData[el_in * mAzimuthCount * AR_COLOR_BANDS + az_in * AR_COLOR_BANDS + 2]
		};
	}

	void save(const std::filesystem::path& path) const;

private:
	std::vector<float> mData;

	size_t mAzimuthCount;
	size_t mElevationCount;
};
} // namespace IG