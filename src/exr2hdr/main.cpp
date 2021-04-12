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
	if (!in)
		return EXIT_FAILURE;
	const ImageSpec& spec = in->spec();
	int xres			  = spec.width;
	int yres			  = spec.height;
	int channels		  = spec.nchannels;
	std::vector<float> pixels(xres * yres * channels);
	in->read_image(TypeDesc::FLOAT, &pixels[0]);
	in->close();

	// Output
	auto out = ImageOutput::create(output);
	if (!out)
		return EXIT_FAILURE;
	out->open(output, spec);
	out->write_image(TypeDesc::FLOAT, &pixels[0]);
	out->close();

	return EXIT_SUCCESS;
}