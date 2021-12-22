#include "Image.h"
#include "ImageIO.h"
#include "Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// We already make use of zlib, so use it here aswell
#include "zlib.h"
#define TINYEXR_USE_THREAD (1)
#define TINYEXR_USE_MINIZ (0)
// #define TINYEXR_IMPLEMENTATION // Alreay included in ImageIO.cpp
#include "tinyexr.h"

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

inline bool ends_with(std::string const& value, std::string const& ending)
{
	if (ending.size() > value.size())
		return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

ImageRgba32 ImageRgba32::load(const std::filesystem::path& path)
{
	std::string ext = path.extension();
	bool useExr		= ends_with(ext, ".exr");

	ImageRgba32 img;

	if (useExr) {
		EXRVersion exr_version;
		int ret = ParseEXRVersionFromFile(&exr_version, path.generic_u8string().c_str());
		if (ret != 0) {
			throw ImageLoadException("Could not extract exr version information", path);
			return {};
		}

		EXRHeader exr_header;
		InitEXRHeader(&exr_header);

		const char* err = nullptr;
		ret				= ParseEXRHeaderFromFile(&exr_header, &exr_version, path.generic_u8string().c_str(), &err);
		if (ret != 0) {
			throw ImageLoadException(err, path);
			FreeEXRErrorMessage(err);
			return {};
		}

		// Make sure exr loads full floating point
		for (int i = 0; i < exr_header.num_channels; i++) {
			if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF)
				exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
		}

		EXRImage exr_image;
		InitEXRImage(&exr_image);
		ret = LoadEXRImageFromFile(&exr_image, &exr_header, path.generic_u8string().c_str(), &err);
		if (ret != TINYEXR_SUCCESS) {
			throw ImageLoadException(err, path);
			FreeEXRErrorMessage(err);
			FreeEXRHeader(&exr_header);
			return {};
		}

		img.width  = exr_image.width;
		img.height = exr_image.height;

		// Extract channel information
		int idxR = -1;
		int idxG = -1;
		int idxB = -1;
		int idxA = -1;
		int idxY = -1;

		int channels = 0;
		for (int c = 0; c < exr_header.num_channels; ++c) {
			size_t len = strlen(exr_header.channels[c].name);
			if (len >= 2)
				continue; // Ignore AOVs

			char channel = exr_header.channels[c].name[len - 1];
			switch (channel) {
			case 'A':
				idxA = c;
				break;
			case 'R':
				idxR = c;
				break;
			case 'G':
				idxG = c;
				break;
			case 'B':
				idxB = c;
				break;
			default:
			case 'Y':
				idxY = c;
				break;
			}
			++channels;
		}

		img.pixels.reset(new float[img.width * img.height * 4]);

		// TODO: Tiled
		if (channels == 1) {
			int idx = idxY != -1 ? idxY : idxA;
			for (size_t i = 0; i < img.width * img.height; ++i) {
				float g				  = reinterpret_cast<float**>(exr_image.images)[idx][i];
				img.pixels[i * 4 + 0] = g;
				img.pixels[i * 4 + 1] = g;
				img.pixels[i * 4 + 2] = g;
				img.pixels[i * 4 + 3] = 1;
			}
		} else {
			if (idxR == -1 || idxG == -1 || idxB == -1) {
				FreeEXRHeader(&exr_header);
				FreeEXRImage(&exr_image);
				return {}; // TODO: Error message
			}

			for (size_t i = 0; i < img.width * img.height; ++i) {
				img.pixels[4 * i + 0] = reinterpret_cast<float**>(exr_image.images)[idxR][i];
				img.pixels[4 * i + 1] = reinterpret_cast<float**>(exr_image.images)[idxG][i];
				img.pixels[4 * i + 2] = reinterpret_cast<float**>(exr_image.images)[idxB][i];
				if (idxA != -1)
					img.pixels[4 * i + 3] = reinterpret_cast<float**>(exr_image.images)[idxA][i];
				else
					img.pixels[4 * i + 3] = 1;
			}
		}

		FreeEXRHeader(&exr_header);
		FreeEXRImage(&exr_image);
	} else {
		stbi_set_unpremultiply_on_load(1);

		int width, height, channels;
		float* data = stbi_loadf(path.generic_u8string().c_str(), &width, &height, &channels, 0);

		if (data == nullptr) {
			throw ImageLoadException("Could not load image", path);
			return {};
		}

		img.width  = width;
		img.height = height;
		img.pixels.reset(new float[img.width * img.height * 4]);

		switch (channels) {
		case 0:
			return ImageRgba32();
		case 1: // Gray
			for (size_t i = 0; i < img.width * img.height; ++i) {
				float g				  = data[i];
				img.pixels[i * 4 + 0] = g;
				img.pixels[i * 4 + 1] = g;
				img.pixels[i * 4 + 2] = g;
				img.pixels[i * 4 + 3] = 1;
			}
			break;
		case 3: // RGB
			for (size_t i = 0; i < img.width * img.height; ++i) {
				img.pixels[i * 4 + 0] = data[i * 3 + 0];
				img.pixels[i * 4 + 1] = data[i * 3 + 1];
				img.pixels[i * 4 + 2] = data[i * 3 + 2];
				img.pixels[i * 4 + 3] = 1;
			}
			break;
		case 4: // RGBA
			std::memcpy(img.pixels.get(), data, sizeof(float) * 4 * img.width * img.height);
			break;
		default:
			for (size_t i = 0; i < img.width * img.height; ++i) {
				img.pixels[i * 4 + 0] = data[i * channels + 1];
				img.pixels[i * 4 + 1] = data[i * channels + 2];
				img.pixels[i * 4 + 2] = data[i * channels + 3];
				img.pixels[i * 4 + 3] = data[i * channels + 0];
			}
			break;
		}
		stbi_image_free(data);
	}

	img.flipY(); // Images loaded seem to tend to be flipped!
	return img;
}

bool ImageRgba32::save(const std::filesystem::path& path)
{
	return save(path, pixels.get(), width, height);
}

bool ImageRgba32::save(const std::filesystem::path& path, const float* rgba, size_t width, size_t height)
{
	std::string ext = path.extension();
	bool useExr		= ends_with(ext, ".exr");

	// We only support .exr output
	if (!useExr)
		return false;

	std::vector<float> images[4];
	images[0].resize(width * height);
	images[1].resize(width * height);
	images[2].resize(width * height);
	images[3].resize(width * height);

	// Split into layers
	for (size_t i = 0; i < width * height; ++i) {
		images[0][i] = rgba[4 * i + 0];
		images[1][i] = rgba[4 * i + 1];
		images[2][i] = rgba[4 * i + 2];
		images[3][i] = rgba[4 * i + 3];
	}

	std::vector<const float*> image_ptrs(4);
	image_ptrs[0] = &(images[3].at(0)); // A
	image_ptrs[1] = &(images[2].at(0)); // B
	image_ptrs[2] = &(images[1].at(0)); // G
	image_ptrs[3] = &(images[0].at(0)); // R

	return ImageIO::save(path, width, height, image_ptrs, { "A", "B", "G", "R" });
}

} // namespace IG