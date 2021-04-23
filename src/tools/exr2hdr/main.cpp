#include <filesystem>
#include <iostream>

#include <OpenImageIO/imageio.h>

using namespace OIIO;
int main(int argc, char** argv)
{
	if (argc != 2) {
		std::cout << "Expected exr2hdr INPUT" << std::endl;
		return EXIT_FAILURE;
	}

	const std::string input	 = argv[1];
	const std::string output = std::filesystem::path(input).replace_extension(".hdr").generic_string();

	// Input
	auto in = ImageInput::open(input);
	if (!in) {
		std::cout << "Could not open " << input << ": " << OIIO::geterror() << std::endl;
		return EXIT_FAILURE;
	}

	const ImageSpec& spec = in->spec();
	int xres			  = spec.width;
	int yres			  = spec.height;
	int channels		  = spec.nchannels;

	int r = std::distance(spec.channelnames.begin(), std::find(spec.channelnames.begin(), spec.channelnames.end(), "R"));
	int g = std::distance(spec.channelnames.begin(), std::find(spec.channelnames.begin(), spec.channelnames.end(), "G"));
	int b = std::distance(spec.channelnames.begin(), std::find(spec.channelnames.begin(), spec.channelnames.end(), "B"));

	std::vector<float> line(xres * channels);
	std::vector<float> pixels(xres * yres * 3);
	for (int y = 0; y < yres; ++y) {
		in->read_scanline(y, 0, &line[0]);
		for (int x = 0; x < xres; ++x) {
			pixels[y * xres * 3 + x * 3 + 0] = line[x * channels + r];
			pixels[y * xres * 3 + x * 3 + 1] = line[x * channels + g];
			pixels[y * xres * 3 + x * 3 + 2] = line[x * channels + b];
		}
	}
	in->close();

	// Output
	auto out = ImageOutput::create(output);
	if (!out) {
		std::cout << "Could not create " << output << ": " << OIIO::geterror() << std::endl;
		return EXIT_FAILURE;
	}

	if (!out->open(output, ImageSpec(xres, yres, 3, TypeFloat))) {
		std::cout << "Could not open " << output << ": " << out->geterror() << std::endl;
		return EXIT_FAILURE;
	}

	out->write_image(TypeFloat, &pixels[0]);
	out->close();
	in->close();

	return EXIT_SUCCESS;
}