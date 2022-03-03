#include "ImageIO.h"
#include "Image.h"

// We already make use of zlib, so use it here aswell
#include "zlib.h"
#define TINYEXR_USE_THREAD (1)
#define TINYEXR_USE_MINIZ (0)
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

namespace IG {

bool ImageIO::save(const std::filesystem::path& path, size_t width, size_t height,
                   const std::vector<const float*>& layer_ptrs, const std::vector<std::string>& layer_names)
{
    IG_ASSERT(layer_ptrs.size() == layer_names.size(), "Expected layer pointers and layer names of the same size");

    // Make sure the directory containing the new file exists
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());

    EXRHeader header;
    InitEXRHeader(&header);
    // header.compression_type = TINYEXR_COMPRESSIONTYPE_ZIP;
    header.compression_type = TINYEXR_COMPRESSIONTYPE_PIZ;

    EXRImage image;
    InitEXRImage(&image);

    image.num_channels = (int)layer_ptrs.size();
    image.images       = (unsigned char**)layer_ptrs.data();
    image.width        = (int)width;
    image.height       = (int)height;

    header.num_channels = image.num_channels;
    header.channels     = new EXRChannelInfo[header.num_channels];

    constexpr size_t BUFFER_MAX = 255;
    for (int i = 0; i < image.num_channels; ++i) {
        const auto& name = layer_names[i];
        strncpy(header.channels[i].name, name.c_str(), BUFFER_MAX);
        if (name.length() >= BUFFER_MAX)
            header.channels[i].name[BUFFER_MAX - 1] = '\0';
    }

    header.pixel_types           = new int[header.num_channels];
    header.requested_pixel_types = new int[header.num_channels];
    for (int i = 0; i < header.num_channels; ++i) {
        header.pixel_types[i]           = TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
        header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of output image to be stored in .EXR
    }

    const char* err = nullptr;
    int ret         = SaveEXRImageToFile(&image, &header, path.generic_u8string().c_str(), &err);

    delete[] header.channels;
    delete[] header.pixel_types;
    delete[] header.requested_pixel_types;

    if (ret != TINYEXR_SUCCESS) {
        throw ImageSaveException(err, path);
        FreeEXRErrorMessage(err); // free's buffer for an error message
        return false;
    }

    return true;
}
} // namespace IG
