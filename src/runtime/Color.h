#pragma once

#include "IG_Config.h"

namespace IG {
struct Color {
    float r = 0, g = 0, b = 0, a = 1;

    inline Color() {}
    inline explicit Color(const Vector3f& rgb)
        : r(rgb(0))
        , g(rgb(1))
        , b(rgb(2))
        , a(1.0f)
    {
    }

    inline Color(float r, float g, float b, float a = 1.0f)
        : r(r)
        , g(g)
        , b(b)
        , a(a)
    {
    }

    inline explicit Color(float x)
        : r(x)
        , g(x)
        , b(x)
        , a(1)
    {
    }

    [[nodiscard]] inline float operator[](size_t index) const
    {
        return reinterpret_cast<const float*>(this)[index];
    }

    inline Color& operator+=(const Color& p)
    {
        r += p.r;
        g += p.g;
        b += p.b;
        return *this;
    }

    [[nodiscard]] inline Color applyGamma(float f = 0.5f) const
    {
        return Color(std::pow(r, f), std::pow(g, f), std::pow(b, f));
    }

    [[nodiscard]] inline Color clamp(const Color& min, const Color& max) const
    {
        return Color(IG::clamp(r, min.r, max.r),
                     IG::clamp(g, min.g, max.b),
                     IG::clamp(b, min.b, max.g),
                     IG::clamp(a, min.a, max.a));
    }

    [[nodiscard]] inline float luminance() const
    {
        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }

    [[nodiscard]] inline float average() const
    {
        return (r + g + b) / 3;
    }

    [[nodiscard]] inline float averageWithAlpha() const
    {
        return (r + g + b + a) / 4;
    }
};
} // namespace IG