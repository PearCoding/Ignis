#include <filesystem>
#include <iostream>

#include "Image.h"
#include "ImageIO.h"

IG_BEGIN_IGNORE_WARNINGS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
IG_END_IGNORE_WARNINGS

using namespace IG;

// Similar to the one in stb, but with meta data
static int write_hdr_core(stbi__write_context* s, int x, int y, int comp, float* data, const std::string_view& cmd_line)
{
    if (y <= 0 || x <= 0 || data == nullptr)
        return 0;
    else {
        // Each component is stored separately. Allocate scratch space for full output scanline.
        unsigned char* scratch = (unsigned char*)STBIW_MALLOC(x * 4);
        int i, len;

        size_t buf_len = 128 + cmd_line.size();
        char* buffer   = (char*)STBIW_MALLOC(buf_len);
        char header[]  = "#?RADIANCE\n# Written by Ignis\nFORMAT=32-bit_rle_rgbe\n";
        s->func(s->context, header, sizeof(header) - 1);

#ifdef __STDC_LIB_EXT1__
        len = sprintf_s(buffer, buf_len, "EXPOSURE=          1.0000000000000\n%s\n-Y %d +X %d\n", cmd_line.data(), y, x);
#else
        len = sprintf(buffer, "EXPOSURE=          1.0000000000000\n%s\n-Y %d +X %d\n", cmd_line.data(), y, x);
#endif
        s->func(s->context, buffer, len);

        for (i = 0; i < y; i++)
            stbiw__write_hdr_scanline(s, x, comp, scratch, data + comp * x * (stbi__flip_vertically_on_write ? y - 1 - i : i));

        STBIW_FREE(buffer);
        STBIW_FREE(scratch);
        return 1;
    }
}

static int write_hdr(char const* filename, int x, int y, int comp, const float* data, const std::string_view& cmd_line)
{
    stbi__write_context s;
    memset(&s, 0, sizeof(s));
    if (stbi__start_write_file(&s, filename)) {
        int r = write_hdr_core(&s, x, y, comp, (float*)data, cmd_line);
        stbi__end_write_file(&s);
        return r;
    } else
        return 0;
}

static std::string genCommandLine(const ImageMetaData& metaData)
{
    Vector3f eye = metaData.CameraEye.value_or(Vector3f::Zero());
    Vector3f dir = metaData.CameraDir.value_or(-Vector3f::UnitZ());
    Vector3f up  = metaData.CameraUp.value_or(Vector3f::UnitY());

    if (metaData.CameraType.value_or("") == "perspective") {
        // TODO: What about other field of views?
        std::stringstream stream;
        stream << "VIEW= -vtv "
               << " -vp " << eye.x() << " " << eye.y() << " " << eye.z()
               << " -vd " << dir.x() << " " << dir.y() << " " << dir.z()
               << " -vu " << up.x() << " " << up.y() << " " << up.z()
               << " -vh 60" << std::endl;
        return stream.str();
    } else if (metaData.CameraType.value_or("") == "fishlens") {
        // TODO: What about other fishlens types?
        std::stringstream stream;
        stream << "VIEW= -vta "
               << " -vp " << eye.x() << " " << eye.y() << " " << eye.z()
               << " -vd " << dir.x() << " " << dir.y() << " " << dir.z()
               << " -vu " << up.x() << " " << up.y() << " " << up.z()
               << " -vh 180 -vv 180" << std::endl;
        return stream.str();
    } else {
        // Not supported
        return {};
    }
}

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) {
        std::cout << "Expected exr2hdr INPUT (OUTPUT)" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string input  = argv[1];
    const std::string output = argc == 3 ? argv[2] : Path(input).replace_extension(".hdr").generic_string();

    try {
        // Input
        ImageMetaData metaData;
        Image image = Image::load(input, &metaData);
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

        std::string cmd_line = genCommandLine(metaData);
        int ret              = write_hdr(output.c_str(), (int)image.width, (int)image.height, 3, data.data(), cmd_line);
        if (ret <= 0)
            return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}