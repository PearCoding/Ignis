#include "Image.h"
#include "ImageUtils.h"

namespace IG {
bool convert_exr(const Path& input, const Path& output, float exposure, float offset, const std::string* optionalInputLayerName)
{
    // Input
    ImageMetaData metaData;
    Image image = Image::load(input, &metaData, optionalInputLayerName);
    if (!image.isValid())
        return false;

    if (exposure != 1 || offset != 0)
        image.applyExposureOffset(exposure, offset);

    // Flip Y
    image.flipY();

    // Output
    return image.save(output, &metaData);
}
} // namespace IG