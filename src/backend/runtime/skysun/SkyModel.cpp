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

    mData.resize(mElevationCount * mAzimuthCount * 4);
    for (size_t k = 0; k < AR_COLOR_BANDS; ++k) {
        const float albedo = ground_albedo[k];

        auto* state = arhosek_rgb_skymodelstate_alloc_init(atmospheric_turbidity, albedo, solar_elevation);
        for (size_t y = 0; y < mElevationCount; ++y) {
            const float theta = std::max(0.001f, ELEVATION_RANGE * y / (float)mElevationCount) - Pi2;
            const float st    = std::sin(theta);
            const float ct    = std::cos(theta);
            for (size_t x = 0; x < mAzimuthCount; ++x) {
                const float azimuth = AZIMUTH_RANGE * x / (float)mAzimuthCount;

                float cosGamma = ct * sun_ce + st * sun_se * std::cos(azimuth - sunEA.Azimuth);
                float gamma    = std::acos(std::min(1.0f, std::max(-1.0f, cosGamma)));
                float radiance = (float)arhosek_tristim_skymodel_radiance(state, theta, gamma, (int)k);

                mData[y * mAzimuthCount * 4 + x * 4 + k] = std::max(0.0f, radiance);
            }
        }

        arhosekskymodelstate_free(state);
    }
}

void SkyModel::save(const std::filesystem::path& path) const
{
    ImageRgba32::save(path, mData.data(), mAzimuthCount, mElevationCount, true);
}
} // namespace IG