#pragma once

#include "IG_Config.h"

namespace IG {
class Colormapper {
public:
    inline Colormapper()
        : mMin(0)
        , mMax(100)
        , mLogarithmic(false)
    {
    }

    inline float map(float v) const
    {
        if (mLogarithmic)
            v = std::log(v + 1e-5f);

        v -= mMin;
        v /= mMax - mMin;
        return std::clamp(v, 0.0f, 1.0f);
    }

    [[nodiscard]] inline float& min() { return mMin; }
    [[nodiscard]] inline float& max() { return mMax; }
    [[nodiscard]] inline bool& isLogarithmic() { return mLogarithmic; }

    Vector4f apply(float v) const;

private:
    float mMin;
    float mMax;
    bool mLogarithmic;
};
} // namespace IG