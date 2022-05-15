#include <filesystem>
#include <iostream>

#include "Image.h"

IG_BEGIN_IGNORE_WARNINGS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
IG_END_IGNORE_WARNINGS

using namespace IG;
inline float gamma(float c)
{
    if (c <= 0.0031308f) {
        return 12.92f * c;
    } else {
        return 1.055f * std::pow(c, 1 / 2.4f) - 0.055f;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) {
        std::cout << "Expected exr2jpg INPUT (OUTPUT)" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string input  = argv[1];
    const std::string output = argc == 3 ? argv[2] : std::filesystem::path(input).replace_extension(".jpg").generic_string();

    try {
        // Input
        Image image = Image::load(input);
        if (!image.isValid())
            return EXIT_FAILURE;

        // Output
        image.flipY();

        std::vector<uint8> data(3 * image.width * image.height);
        for (size_t i = 0; i < image.width * image.height; ++i) {
            data[i * 3 + 0] = static_cast<uint8>(std::max(0.0f, std::min(255.0f, gamma(image.pixels[i * 4 + 0]) * 255)));
            data[i * 3 + 1] = static_cast<uint8>(std::max(0.0f, std::min(255.0f, gamma(image.pixels[i * 4 + 1]) * 255)));
            data[i * 3 + 2] = static_cast<uint8>(std::max(0.0f, std::min(255.0f, gamma(image.pixels[i * 4 + 2]) * 255)));
        }
        int ret = stbi_write_jpg(output.c_str(), (int)image.width, (int)image.height, 3, data.data(), 90);
        if (ret <= 0)
            return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}