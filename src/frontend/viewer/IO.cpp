#include "IO.h"
#include "Image.h"
#include "Range.h"

#ifndef IG_NO_EXECUTION_H
#include <execution>
#endif

namespace IG {
bool saveImageRGB(const std::filesystem::path& path, const float* rgb, size_t width, size_t height, float scale)
{
	ImageRgba32 img;
	img.width  = width;
	img.height = height;
	img.pixels.reset(new float[width * height * 4]);

	const RangeS imageRange(0, width * height);

	const auto pixelF = [&](size_t ind) {
		auto r = rgb[ind * 3 + 0];
		auto g = rgb[ind * 3 + 1];
		auto b = rgb[ind * 3 + 2];

		img.pixels[4 * ind + 0] = r * scale;
		img.pixels[4 * ind + 1] = g * scale;
		img.pixels[4 * ind + 2] = b * scale;
		img.pixels[4 * ind + 3] = 1.0f;
	};

#ifndef IG_NO_EXECUTION_H
	std::for_each(std::execution::par_unseq, imageRange.begin(), imageRange.end(), pixelF);
#else
	for (size_t i : imageRange)
		pixelF(i);
#endif

	return img.save(path);
}

bool saveImageRGBA(const std::filesystem::path& path, const float* rgb, size_t width, size_t height, float scale)
{
	ImageRgba32 img;
	img.width  = width;
	img.height = height;
	img.pixels.reset(new float[width * height * 4]);

	const RangeS imageRange(0, width * height);
	const auto pixelF = [&](size_t ind) {
		auto r = rgb[ind * 4 + 0];
		auto g = rgb[ind * 4 + 1];
		auto b = rgb[ind * 4 + 2];
		auto a = rgb[ind * 4 + 3];

		img.pixels[4 * ind + 0] = r * scale;
		img.pixels[4 * ind + 1] = g * scale;
		img.pixels[4 * ind + 2] = b * scale;
		img.pixels[4 * ind + 3] = a * scale;
	};

#ifndef IG_NO_EXECUTION_H
	std::for_each(std::execution::par_unseq, imageRange.begin(), imageRange.end(), pixelF);
#else
	for (size_t i : imageRange)
		pixelF(i);
#endif

	return img.save(path);
}
} // namespace IG