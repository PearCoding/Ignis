#include <filesystem>
#include <iostream>

#include "Image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace IG;
int main(int argc, char** argv)
{
	if (argc != 2 && argc != 3) {
		std::cout << "Expected exr2hdr INPUT (OUTPUT)" << std::endl;
		return EXIT_FAILURE;
	}

	const std::string input	 = argv[1];
	const std::string output = argc == 3 ? argv[2] : std::filesystem::path(input).replace_extension(".hdr").generic_string();

	try {
		// Input
		ImageRgba32 image = ImageRgba32::load(input);
		if (!image.isValid())
			return EXIT_FAILURE;

		// Output
		image.flipY();

		std::vector<float> data(3 * image.width * image.height);
		for (size_t i = 0; i < image.width * image.height; ++i) {
			data[i * 3 + 0] = image.pixels[i * 4 + 0];
			data[i * 3 + 1] = image.pixels[i * 4 + 1];
			data[i * 3 + 2] = image.pixels[i * 4 + 2];
		}
		int ret = stbi_write_hdr(output.c_str(), image.width, image.height, 3, data.data());
		if (ret <= 0)
			return EXIT_FAILURE;
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}