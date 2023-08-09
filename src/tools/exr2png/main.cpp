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

static inline void check_arg(int argc, char** argv, int arg, int n)
{
    if (arg + n >= argc)
        std::cerr << "Option '" << argv[arg] << "' expects " << n << " arguments, got " << (argc - arg) << std::endl;
}

static inline void version()
{
    std::cout << "exr2png 0.1" << std::endl;
}

static inline void usage()
{
    std::cout
        << "exr2png - Tool to convert from EXR to PNG" << std::endl
        << "Usage: exr2png [options] file" << std::endl
        << "Available options:" << std::endl
        << "   -h      --help             Shows this message" << std::endl
        << "           --version          Show version and exit" << std::endl
        << "   -e      --exposure  value  Apply exposure" << std::endl
        << "   -o      --offset    value  Apply offset" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc <= 1) {
        usage();
        return EXIT_FAILURE;
    }

    Path in_file;
    Path out_file;

    float exposure = 1;
    float offset   = 0;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                usage();
                return EXIT_SUCCESS;
            } else if (!strcmp(argv[i], "--version")) {
                version();
                return EXIT_SUCCESS;
            } else if (!strcmp(argv[i], "-e") || !strcmp(argv[i], "--exposure")) {
                check_arg(argc, argv, i, 1);
                exposure = std::stof(argv[i + 1]);
                i += 1;
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--offset")) {
                check_arg(argc, argv, i, 1);
                offset = std::stof(argv[i + 1]);
                i += 1;
            } else {
                std::cerr << "Unknown option '" << argv[i] << "'" << std::endl;
                usage();
                return EXIT_FAILURE;
            }
        } else {
            if (in_file.empty()) {
                in_file = argv[i];
            } else {
                if (out_file.empty()) {
                    out_file = argv[i];
                } else {
                    std::cerr << "Unexpected argument '" << argv[i] << "'" << std::endl;
                    usage();
                    return EXIT_FAILURE;
                }
            }
        }
    }

    if (out_file.empty())
        out_file = Path(in_file).replace_extension(".png");

    try {
        // Input
        Image image = Image::load(in_file);
        if (!image.isValid())
            return EXIT_FAILURE;

        if (exposure != 1 || offset != 0)
            image.applyExposureOffset(exposure, offset);

        // Output
        image.flipY();

        std::vector<uint8> data(3 * image.width * image.height);
        for (size_t i = 0; i < image.width * image.height; ++i) {
            data[i * 3 + 0] = static_cast<uint8>(std::max(0.0f, std::min(255.0f, gamma(image.pixels[i * 4 + 0]) * 255)));
            data[i * 3 + 1] = static_cast<uint8>(std::max(0.0f, std::min(255.0f, gamma(image.pixels[i * 4 + 1]) * 255)));
            data[i * 3 + 2] = static_cast<uint8>(std::max(0.0f, std::min(255.0f, gamma(image.pixels[i * 4 + 2]) * 255)));
        }
        int ret = stbi_write_png(out_file.generic_string().c_str(), (int)image.width, (int)image.height, 3, data.data(), static_cast<int>(sizeof(uint8) * 3 * image.width));
        if (ret <= 0)
            return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}