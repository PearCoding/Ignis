#include "Image.h"
#include "Logger.h"

#include <OpenImageIO/imageio.h>
using namespace OIIO;

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
	auto in = ImageInput::open(path);
	if (!in)
		return ImageRgba32();
	const ImageSpec& spec = in->spec();

	ImageRgba32 img;
	img.width  = spec.width;
	img.height = spec.height;
	img.pixels.reset(new float[img.width * img.height * 4]);

	switch (spec.nchannels) {
	case 1: { // Gray
		std::vector<float> scanline(spec.width * spec.nchannels);
		for (int y = 0; y < spec.height; ++y) {
			in->read_scanline(y, 0, TypeDesc::FLOAT, &scanline[0]);
			for (int x = 0; x < spec.width; ++x) {
				float g									   = scanline[x];
				img.pixels[y * spec.width * 4 + x * 4 + 0] = g;
				img.pixels[y * spec.width * 4 + x * 4 + 1] = g;
				img.pixels[y * spec.width * 4 + x * 4 + 2] = g;
				img.pixels[y * spec.width * 4 + x * 4 + 3] = 1;
			}
		}
	} break;
	case 3: { // RGB
		std::vector<float> scanline(spec.width * spec.nchannels);
		for (int y = 0; y < spec.height; ++y) {
			in->read_scanline(y, 0, TypeDesc::FLOAT, &scanline[0]);
			for (int x = 0; x < spec.width; ++x) {
				img.pixels[y * spec.width * 4 + x * 4 + 0] = scanline[3 * x + 0];
				img.pixels[y * spec.width * 4 + x * 4 + 1] = scanline[3 * x + 1];
				img.pixels[y * spec.width * 4 + x * 4 + 2] = scanline[3 * x + 2];
				img.pixels[y * spec.width * 4 + x * 4 + 3] = 1;
			}
		}
	} break;
	case 4: // RGBA
		in->read_image(TypeDesc::FLOAT, img.pixels.get());
		break;
	default:
		return ImageRgba32();
	}

	in->close();
	return img;
}

bool ImageRgba32::save(const std::filesystem::path& path)
{
	auto out = ImageOutput::create(path);
	if (!out)
		return false;

	ImageSpec spec(width, height, 4, TypeDesc::FLOAT);
	out->open(path, spec);
	out->write_image(TypeDesc::FLOAT, pixels.get());
	out->close();

	return true;
}
} // namespace IG