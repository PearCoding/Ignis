#pragma once
#include "IG_Config.h"

namespace IG {
enum class ConvertToStdImage {
    BMP,
    JPG,
    PNG,
    TGA,
};

bool convert_stb(const Path& input, const Path& output, ConvertToStdImage type, float exposure, float offset, int jpg_quality, const std::string* optionalInputLayerName);
bool convert_hdr(const Path& input, const Path& output, float exposure, float offset, const std::string* optionalInputLayerName);
bool convert_exr(const Path& input, const Path& output, float exposure, float offset, const std::string* optionalInputLayerName);
bool dump_metadata(const Path& input);
} // namespace IG