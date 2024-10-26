#include "Image.h"
#include "ImageUtils.h"

#include <fstream>

IG_BEGIN_IGNORE_WARNINGS
// #define STBI_WRITE_NO_STDIO (needed for stbiw__write_hdr_scanline)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
IG_END_IGNORE_WARNINGS

namespace IG {

static void sWriteFunc(void* context, void* data, int size)
{
    ((std::ofstream*)context)->write((const char*)data, size);
};

static inline float gamma(float c)
{
    if (c <= 0.0031308f) {
        return 12.92f * c;
    } else {
        return 1.055f * std::pow(c, 1 / 2.4f) - 0.055f;
    }
}

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

static int write_hdr(stbi_write_func* func, void* context, int x, int y, int comp, const float* data, const std::string_view& cmd_line)
{
    stbi__write_context s;
    memset(&s, 0, sizeof(s));
    stbi__start_write_callbacks(&s, func, context);
    return write_hdr_core(&s, x, y, comp, (float*)data, cmd_line);
}

static std::string genCommandLine(const ImageMetaData& metaData)
{
    const Vector3f eye = metaData.CameraEye.value_or(Vector3f::Zero());
    const Vector3f dir = metaData.CameraDir.value_or(-Vector3f::UnitZ());
    const Vector3f up  = metaData.CameraUp.value_or(Vector3f::UnitY());

    const std::string camera_type = metaData.CameraType.value_or("");
    if (camera_type == "perspective") {
        // TODO: What about other field of views?
        std::stringstream stream;
        stream << "VIEW= -vtv "
               << " -vp " << eye.x() << " " << eye.y() << " " << eye.z()
               << " -vd " << dir.x() << " " << dir.y() << " " << dir.z()
               << " -vu " << up.x() << " " << up.y() << " " << up.z()
               << " -vh 60" << std::endl;
        return stream.str();
    } else if (camera_type == "fishlens" || camera_type == "fisheye") {
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

bool convert_stb(const Path& input, const Path& output, ConvertToStdImage type, float exposure, float offset, int jpg_quality)
{
    // Input
    Image image = Image::load(input);
    if (!image.isValid())
        return false;

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

    std::ofstream stream(output, std::ios::binary | std::ios::binary);

    int ret = 0;
    switch (type) {
    case ConvertToStdImage::PNG:
        ret = stbi_write_png_to_func(sWriteFunc, (void*)&stream, (int)image.width, (int)image.height, 3, data.data(), static_cast<int>(sizeof(uint8) * 3 * image.width));
        break;
    case ConvertToStdImage::JPG:
        ret = stbi_write_jpg_to_func(sWriteFunc, (void*)&stream, (int)image.width, (int)image.height, 3, data.data(), jpg_quality);
        break;
    case ConvertToStdImage::BMP:
        ret = stbi_write_bmp_to_func(sWriteFunc, (void*)&stream, (int)image.width, (int)image.height, 3, data.data());
        break;
    case ConvertToStdImage::TGA:
        ret = stbi_write_tga_to_func(sWriteFunc, (void*)&stream, (int)image.width, (int)image.height, 3, data.data());
        break;
    default:
        IG_ASSERT(false, "Invalid type!");
    }

    return ret > 0;
}

bool convert_hdr(const Path& input, const Path& output, float exposure, float offset)
{
    // Input
    ImageMetaData metaData;
    Image image = Image::load(input, &metaData);
    if (!image.isValid())
        return false;

    if (exposure != 1 || offset != 0)
        image.applyExposureOffset(exposure, offset);

    // Output
    image.flipY();

    std::vector<float> data(3 * image.width * image.height);
    for (size_t i = 0; i < image.width * image.height; ++i) {
        data[i * 3 + 0] = image.pixels[i * 4 + 0];
        data[i * 3 + 1] = image.pixels[i * 4 + 1];
        data[i * 3 + 2] = image.pixels[i * 4 + 2];
    }

    std::ofstream stream(output, std::ios::binary | std::ios::binary);

    std::string cmd_line = genCommandLine(metaData);
    int ret              = write_hdr(sWriteFunc, (void*)&stream, (int)image.width, (int)image.height, 3, data.data(), cmd_line);
    return ret > 0;
}
} // namespace IG