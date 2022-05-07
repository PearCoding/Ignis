#include "SkyModel.h"
#include "Image.h"
#include "SunLocation.h"
#include "model/ArHosekSkyModel.h"
#include "serialization/Serializer.h"

namespace IG {
SkyModel::SkyModel(const RGB& ground_albedo, const ElevationAzimuth& sunEA, float turbidity, size_t resAzimuth, size_t resElevation)
    : mAzimuthCount(resAzimuth)
    , mElevationCount(resElevation)
{
    const float solar_elevation       = Pi2 - sunEA.Elevation;
    const float atmospheric_turbidity = turbidity;

    const float sun_se = std::sin(solar_elevation);
    const float sun_ce = std::cos(solar_elevation);

    std::array<ArHosekSkyModelState*, AR_COLOR_BANDS> states;
    for (size_t k = 0; k < AR_COLOR_BANDS; ++k)
        states[k] = arhosek_rgb_skymodelstate_alloc_init(atmospheric_turbidity, ground_albedo[k], solar_elevation);

    mData.resize(mElevationCount * mAzimuthCount * 4);

    for (size_t y = 0; y < mElevationCount; ++y) {
        const float theta = ELEVATION_RANGE * y / (float)mElevationCount;
        const float st    = std::sin(theta);
        const float ct    = std::cos(theta);
        for (size_t x = 0; x < mAzimuthCount; ++x) {
            float azimuth = AZIMUTH_RANGE * x / (float)mAzimuthCount - Pi4;
            if (azimuth < 0)
                azimuth += 2 * Pi;

            const float cosGamma = ct * sun_ce + st * sun_se * std::cos(azimuth - sunEA.Azimuth);
            const float gamma    = std::acos(std::min(1.0f, std::max(-1.0f, cosGamma)));

            for (size_t k = 0; k < AR_COLOR_BANDS; ++k) {
                const float radiance = (float)arhosek_tristim_skymodel_radiance(states[k], theta, gamma, (int)k);

                mData[y * mAzimuthCount * 4 + x * 4 + k] = std::max(0.0f, radiance);
            }
        }
    }

    for (const auto& state : states)
        arhosekskymodelstate_free(state);
}

void SkyModel::save(const std::filesystem::path& path) const
{
    Image::save(path, mData.data(), mAzimuthCount, mElevationCount, true);
}
} // namespace IG