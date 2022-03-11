#pragma once

#include "IG_Config.h"

namespace IG {
struct RGBA;

struct RGB {
    float r, g, b;

    inline RGB() {}
    inline explicit RGB(const Vector3f& rgb)
        : r(rgb(0))
        , g(rgb(1))
        , b(rgb(2))
    {
    }

    inline RGB(float r, float g, float b)
        : r(r)
        , g(g)
        , b(b)
    {
    }

    inline explicit RGB(float x)
        : r(x)
        , g(x)
        , b(x)
    {
    }

    inline explicit RGB(const RGBA& rgba);

    inline float operator[](size_t index) const
    {
        return reinterpret_cast<const float*>(this)[index];
    }

    inline RGB& operator+=(const RGB& p)
    {
        r += p.r;
        g += p.g;
        b += p.b;
        return *this;
    }

    inline RGB applyGamma(float f = 0.5f) const
    {
        return RGB(std::pow(r, f), std::pow(g, f), std::pow(b, f));
    }

    inline RGB clamp(const RGB& min, const RGB& max) const
    {
        return RGB(IG::clamp(r, min.r, max.r),
                   IG::clamp(g, min.g, max.b),
                   IG::clamp(b, min.b, max.g));
    }

    inline float luminance() const
    {
        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }
};

struct RGBA {
    float r, g, b, a;

    inline RGBA() {}
    inline explicit RGBA(const Vector4f& rgba)
        : r(rgba(0))
        , g(rgba(1))
        , b(rgba(2))
        , a(rgba(3))
    {
    }

    inline RGBA(float r, float g, float b, float a)
        : r(r)
        , g(g)
        , b(b)
        , a(a)
    {
    }

    inline explicit RGBA(float x)
        : r(x)
        , g(x)
        , b(x)
        , a(x)
    {
    }

    inline explicit RGBA(const RGB& rgb, float a)
        : r(rgb.r)
        , g(rgb.g)
        , b(rgb.b)
        , a(a)
    {
    }

    inline RGBA& operator+=(const RGBA& p)
    {
        r += p.r;
        g += p.g;
        b += p.b;
        a += p.a;
        return *this;
    }

    inline RGBA applyGamma(float f = 0.5f) const
    {
        return RGBA(std::pow(r, f), std::pow(g, f), std::pow(b, f), a);
    }

    inline RGBA clamp(const RGBA& min, const RGBA& max) const
    {
        return RGBA(IG::clamp(r, min.r, max.r),
                    IG::clamp(g, min.g, max.b),
                    IG::clamp(b, min.b, max.g),
                    IG::clamp(a, min.a, max.a));
    }
};

inline RGB::RGB(const RGBA& rgba)
    : r(rgba.r)
    , g(rgba.g)
    , b(rgba.b)
{
}
} // namespace IG