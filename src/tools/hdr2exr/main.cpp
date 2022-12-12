#include <filesystem>
#include <iostream>

#include "Image.h"

using namespace IG;
int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) {
        std::cout << "Expected hdr2exr INPUT (OUTPUT)" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string input  = argv[1];
    const std::string output = argc == 3 ? argv[2] : std::filesystem::path(input).replace_extension(".exr").generic_string();

    try {
        // Input
        Image image = Image::load(input);
        if (!image.isValid())
            return EXIT_FAILURE;

        // Flip Y
        image.flipY();

        // Output
        if (!image.save(output))
            return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}