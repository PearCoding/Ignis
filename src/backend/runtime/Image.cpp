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

void ImageRgba32::flipY()
{
	const size_t slice = 4 * width;
	for (size_t y = 0; y < height / 2; ++y) {
		float* s1 = &pixels[y * slice];
		float* s2 = &pixels[(height - y - 1) * slice];
		if (s1 != s2)
			std::swap_ranges(s1, s1 + slice, s2);
	}
}

ImageRgba32 ImageRgba32::load(const std::filesystem::path& path)
{
	auto in = ImageInput::open(path.generic_u8string());
	if (!in)
		return ImageRgba32();
	const ImageSpec& spec = in->spec();

	ImageRgba32 img;
	img.width  = spec.width;
	img.height = spec.height;
	img.pixels.reset(new float[img.width * img.height * 4]);

	bool wasBGR = false;
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
		wasBGR = spec.channelnames[0] == "B";
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
		wasBGR = spec.channelnames[0] == "B";
		in->read_image(TypeDesc::FLOAT, img.pixels.get());
		break;
	default:
		return ImageRgba32();
	}

	if (wasBGR) {
		for (int i = 0; i < spec.height * spec.width; ++i)
			std::swap(img.pixels[4 * i + 0], img.pixels[4 * i + 2]);
	}

	img.flipY();

	in->close();
	return img;
}

bool ImageRgba32::save(const std::filesystem::path& path)
{
	return save(path, pixels.get(), width, height);
}

bool ImageRgba32::save(const std::filesystem::path& path, const float* rgb, size_t width, size_t height)
{
	auto out = ImageOutput::create(path.generic_u8string());
	if (!out)
		return false;

	ImageSpec spec(width, height, 4, TypeDesc::FLOAT);
	out->open(path, spec);
	out->write_image(TypeDesc::FLOAT, rgb);
	out->close();

	return true;
}

///////////////////////////////////////

void ImageRgb24::applyGammaCorrection()
{
	IG_ASSERT(isValid(), "Expected valid image");

	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			auto* pix = &pixels[3 * (y * width + x)];
			for (int i = 0; i < 3; ++i)
				pix[i] = std::pow(pix[i], 2.2f);
		}
	}
}

void ImageRgb24::flipY()
{
	const size_t slice = 3 * width;
	for (size_t y = 0; y < height / 2; ++y) {
		float* s1 = &pixels[y * slice];
		float* s2 = &pixels[(height - y - 1) * slice];
		if (s1 != s2)
			std::swap_ranges(s1, s1 + slice, s2);
	}
}

ImageRgb24 ImageRgb24::load(const std::filesystem::path& path)
{
	auto in = ImageInput::open(path);
	if (!in)
		return ImageRgb24();
	const ImageSpec& spec = in->spec();

	ImageRgb24 img;
	img.width  = spec.width;
	img.height = spec.height;
	img.pixels.reset(new float[img.width * img.height * 3]);

	bool wasBGR = false;
	switch (spec.nchannels) {
	case 1: { // Gray
		std::vector<float> scanline(spec.width * spec.nchannels);
		for (int y = 0; y < spec.height; ++y) {
			in->read_scanline(y, 0, TypeDesc::FLOAT, &scanline[0]);
			for (int x = 0; x < spec.width; ++x) {
				float g									   = scanline[x];
				img.pixels[y * spec.width * 3 + x * 3 + 0] = g;
				img.pixels[y * spec.width * 3 + x * 3 + 1] = g;
				img.pixels[y * spec.width * 3 + x * 3 + 2] = g;
			}
		}
	} break;
	case 3: // RGB
		wasBGR = spec.channelnames[0] == "B";
		in->read_image(TypeDesc::FLOAT, img.pixels.get());
		break;
	case 4: { // RGBA
		wasBGR = spec.channelnames[0] == "B";
		std::vector<float> scanline(spec.width * spec.nchannels);
		for (int y = 0; y < spec.height; ++y) {
			in->read_scanline(y, 0, TypeDesc::FLOAT, &scanline[0]);
			for (int x = 0; x < spec.width; ++x) {
				img.pixels[y * spec.width * 3 + x * 3 + 0] = scanline[4 * x + 0];
				img.pixels[y * spec.width * 3 + x * 3 + 1] = scanline[4 * x + 1];
				img.pixels[y * spec.width * 3 + x * 3 + 2] = scanline[4 * x + 2];
			}
		}
	} break;
	default:
		return ImageRgb24();
	}

	if (wasBGR) {
		for (int i = 0; i < spec.height * spec.width; ++i)
			std::swap(img.pixels[3 * i + 0], img.pixels[3 * i + 2]);
	}

	img.flipY();

	in->close();
	return img;
}

bool ImageRgb24::save(const std::filesystem::path& path)
{
	return save(path, pixels.get(), width, height);
}

bool ImageRgb24::save(const std::filesystem::path& path, const float* rgb, size_t width, size_t height)
{
	auto out = ImageOutput::create(path.generic_u8string());
	if (!out)
		return false;

	ImageSpec spec(width, height, 3, TypeDesc::FLOAT);
	out->open(path, spec);
	out->write_image(TypeDesc::FLOAT, rgb);
	out->close();

	return true;
}
} // namespace IG