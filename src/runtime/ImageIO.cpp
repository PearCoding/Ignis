#include "ImageIO.h"
#include "Image.h"

#include <iterator>
#include <numeric>

IG_BEGIN_IGNORE_WARNINGS
// We already make use of zlib, so use it here aswell
#include <zlib.h>
#define TINYEXR_USE_THREAD (1)
#define TINYEXR_USE_MINIZ (0)
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>
IG_END_IGNORE_WARNINGS

namespace IG {
// attr.value has to be deleted!
EXRAttribute makeStringAttribute(const std::string_view& name, const std::string_view& data)
{
    EXRAttribute attr;
    strcpy(attr.name, name.data());
    strcpy(attr.type, "string");

    int len    = (int)data.size();
    attr.value = new unsigned char[len + sizeof(int)];
    memcpy(attr.value, reinterpret_cast<unsigned char*>(&len), sizeof(len));
    strncpy(reinterpret_cast<char*>(attr.value + sizeof(len)), data.data(), len);

    attr.size = sizeof(len) + len;

    return attr;
}

// attr.value has to be deleted!
EXRAttribute makeVec3Attribute(const std::string_view& name, const Vector3f& data)
{
    EXRAttribute attr;
    strcpy(attr.name, name.data());
    strcpy(attr.type, "v3f");

    const float xyz[3] = { data.x(), data.y(), data.z() };
    attr.value         = new unsigned char[3 * sizeof(float)];
    memcpy(attr.value, reinterpret_cast<const unsigned char*>(xyz), 3 * sizeof(float));

    attr.size = 3 * sizeof(float);

    return attr;
}

// attr.value has to be deleted!
EXRAttribute makeIntAttribute(const std::string_view& name, int data)
{
    EXRAttribute attr;
    strcpy(attr.name, name.data());
    strcpy(attr.type, "int");

    attr.value = new unsigned char[sizeof(data)];
    memcpy(attr.value, reinterpret_cast<unsigned char*>(&data), sizeof(data));

    attr.size = sizeof(data);

    return attr;
}

bool ImageIO::save(const Path& path, size_t width, size_t height,
                   const std::vector<const float*>& layer_ptrs, const std::vector<std::string>& layer_names,
                   const ImageMetaData& metaData)
{
    IG_ASSERT(layer_ptrs.size() == layer_names.size(), "Expected layer pointers and layer names of the same size");

    // Make sure the directory containing the new file exists
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());

    // Sort layers
    std::vector<size_t> layer_indices(layer_names.size());
    std::iota(std::begin(layer_indices), std::end(layer_indices), 0);
    std::sort(std::begin(layer_indices), std::end(layer_indices), [&](size_t a, size_t b) { return layer_names[a] < layer_names[b]; });

    std::vector<const float*> sorted_layer_ptrs(layer_ptrs.size());
    for (size_t i = 0; i < layer_indices.size(); ++i)
        sorted_layer_ptrs[i] = layer_ptrs[layer_indices[i]];

    // Init
    EXRHeader header;
    InitEXRHeader(&header);
    // header.compression_type = TINYEXR_COMPRESSIONTYPE_ZIP;
    header.compression_type = TINYEXR_COMPRESSIONTYPE_PIZ;

    EXRImage image;
    InitEXRImage(&image);

    image.num_channels = (int)sorted_layer_ptrs.size();
    image.images       = (unsigned char**)sorted_layer_ptrs.data();
    image.width        = (int)width;
    image.height       = (int)height;

    header.num_channels = image.num_channels;
    header.channels     = new EXRChannelInfo[header.num_channels];

    constexpr size_t BUFFER_MAX = 255;
    for (int i = 0; i < image.num_channels; ++i) {
        const auto& name = layer_names[layer_indices[i]];
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

    // Handle meta data
    std::vector<EXRAttribute> attributes;
    if (metaData.CameraType.has_value())
        attributes.emplace_back(makeStringAttribute("igCameraType", metaData.CameraType.value()));
    if (metaData.TechniqueType.has_value())
        attributes.emplace_back(makeStringAttribute("igTechniqueType", metaData.TechniqueType.value()));
    if (metaData.TargetString.has_value())
        attributes.emplace_back(makeStringAttribute("igTarget", metaData.TargetString.value()));
    if (metaData.CameraEye.has_value())
        attributes.emplace_back(makeVec3Attribute("igCameraEye", metaData.CameraEye.value()));
    if (metaData.CameraUp.has_value())
        attributes.emplace_back(makeVec3Attribute("igCameraUp", metaData.CameraUp.value()));
    if (metaData.CameraDir.has_value())
        attributes.emplace_back(makeVec3Attribute("igCameraDir", metaData.CameraDir.value()));
    if (metaData.Seed.has_value())
        attributes.emplace_back(makeIntAttribute("igSeed", (int)metaData.Seed.value()));
    if (metaData.SamplePerPixel.has_value())
        attributes.emplace_back(makeIntAttribute("igSPP", (int)metaData.SamplePerPixel.value()));
    if (metaData.SamplePerIteration.has_value())
        attributes.emplace_back(makeIntAttribute("igSPI", (int)metaData.SamplePerIteration.value()));
    if (metaData.Iteration.has_value())
        attributes.emplace_back(makeIntAttribute("igIteration", (int)metaData.Iteration.value()));
    if (metaData.Frame.has_value())
        attributes.emplace_back(makeIntAttribute("igFrame", (int)metaData.Frame.value()));
    if (metaData.RendertimeInSeconds.has_value())
        attributes.emplace_back(makeIntAttribute("igRendertime", (int)metaData.RendertimeInSeconds.value()));

    if (!attributes.empty()) {
        header.custom_attributes     = attributes.data();
        header.num_custom_attributes = (int)attributes.size();
    }

    const char* err = nullptr;
    int ret         = SaveEXRImageToFile(&image, &header, path.generic_u8string().c_str(), &err);

    for (auto& attr : attributes)
        delete[] attr.value;

    delete[] header.channels;
    delete[] header.pixel_types;
    delete[] header.requested_pixel_types;

    if (ret != TINYEXR_SUCCESS) {
        std::string _err = err;
        FreeEXRErrorMessage(err); // free's buffer for an error message
        throw ImageSaveException(_err, path);
        return false;
    }

    return true;
}
} // namespace IG
