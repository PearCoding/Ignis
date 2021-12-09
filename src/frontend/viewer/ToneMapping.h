#pragma once

#include "Color.h"

namespace IG {
// ToneMapping
#define RGB_C(r, g, b) (((r) << 16) | ((g) << 8) | (b))

inline RGB xyz_to_srgb(const RGB& c)
{
	return RGB(3.2404542f * c.r - 1.5371385f * c.g - 0.4985314f * c.b,
			   -0.9692660f * c.r + 1.8760108f * c.g + 0.0415560f * c.b,
			   0.0556434f * c.r - 0.2040259f * c.g + 1.0572252f * c.b);
}

inline RGB srgb_to_xyz(const RGB& c)
{
	return RGB(0.4124564f * c.r + 0.3575761f * c.g + 0.1804375f * c.b,
			   0.2126729f * c.r + 0.7151522f * c.g + 0.0721750f * c.b,
			   0.0193339f * c.r + 0.1191920f * c.g + 0.9503041f * c.b);
}

inline RGB xyY_to_srgb(const RGB& c)
{
	return c.g == 0 ? RGB(0, 0, 0) : xyz_to_srgb(RGB(c.r * c.b / c.g, c.b, (1 - c.r - c.g) * c.b / c.g));
}

inline RGB srgb_to_xyY(const RGB& c)
{
	const auto s = srgb_to_xyz(c);
	const auto n = s.r + s.g + s.b;
	return (n == 0) ? RGB(0, 0, 0) : RGB(s.r / n, s.g / n, s.g);
}

inline float reinhard(float L)
{
	return L / (1.0f + L);
}

inline float reinhard_modified(float L)
{
	constexpr float WhitePoint = 4.0f;
	return (L * (1.0f + L / (WhitePoint * WhitePoint))) / (1.0f + L);
}

inline float aces(float L)
{
	constexpr float a = 2.51f;
	constexpr float b = 0.03f;
	constexpr float c = 2.43f;
	constexpr float d = 0.59f;
	constexpr float e = 0.14f;
	return std::min(1.0f, std::max(0.0f, (L * (a * L + b)) / (L * (c * L + d) + e)));
}

inline float srgb_gamma(float x)
{
	if (x <= 0.0031308f)
		return 12.92f * x;
	else
		return 1.055f * std::pow(x, 0.416666667f) - 0.055f;
}

template <typename T>
void updateMaximum(std::atomic<T>& maximum_value, T const& value) noexcept
{
	T prev_value = maximum_value;
	while (prev_value < value && !maximum_value.compare_exchange_weak(prev_value, value)) {
	}
}

template <typename T>
void updateMinimum(std::atomic<T>& minimum_value, T const& value) noexcept
{
	T prev_value = minimum_value;
	while (prev_value > value && !minimum_value.compare_exchange_weak(prev_value, value)) {
	}
}
} // namespace IG