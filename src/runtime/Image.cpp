#include "Image.h"

namespace IG {
void ImageRgba32::applyGammaCorrection()
{
	IG_ASSERT(isValid(), "Expected valid image");

	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			auto* pix = &pixels[4 * (y * width + x)];
			for (int i = 0; i < 3; ++i)
				pix[i] = std::pow(pix[i], 2.2f);
		}
	}
}

ImageRgba32 ImageRgba32::load(const std::filesystem::path& path)
{
	return ImageRgba32(); //TODO
}

bool ImageRgba32::save(const std::filesystem::path& path)
{
	return false; // TODO
}
} // namespace IG