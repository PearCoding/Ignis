#include "Image.h"
#include "ImageUtils.h"
#include "Logger.h"

namespace IG {
bool dump_metadata(const Path& input)
{
    // Input
    ImageMetaData metaData;
    Image image = Image::load(input, &metaData);
    if (!image.isValid())
        return EXIT_FAILURE;

    if (metaData.Seed.has_value())
        std::cout << "Seed:       " << metaData.Seed.value() << std::endl;
    if (metaData.SamplePerPixel.has_value())
        std::cout << "SPP:        " << metaData.SamplePerPixel.value() << std::endl;
    if (metaData.SamplePerIteration.has_value())
        std::cout << "SPI:        " << metaData.SamplePerIteration.value() << std::endl;
    if (metaData.Iteration.has_value())
        std::cout << "Iterations: " << metaData.Iteration.value() << std::endl;
    if (metaData.Frame.has_value())
        std::cout << "Frames:     " << metaData.Frame.value() << std::endl;
    if (metaData.RendertimeInMilliseconds.has_value())
        std::cout << "Rendertime: " << metaData.RendertimeInMilliseconds.value() << "ms" << std::endl;
    else if (metaData.RendertimeInSeconds.has_value())
        std::cout << "Rendertime: " << metaData.RendertimeInSeconds.value() << "s" << std::endl;
    if (metaData.TechniqueType.has_value())
        std::cout << "Technique:  " << metaData.TechniqueType.value() << std::endl;
    if (metaData.TargetString.has_value())
        std::cout << "Device:     " << metaData.TargetString.value() << std::endl;
    if (metaData.CameraType.has_value())
        std::cout << "Camera:     " << metaData.CameraType.value() << std::endl;
    if (metaData.CameraEye.has_value())
        std::cout << "  Eye:      " << FormatVector(metaData.CameraEye.value()) << std::endl;
    if (metaData.CameraUp.has_value())
        std::cout << "  Up:       " << FormatVector(metaData.CameraUp.value()) << std::endl;
    if (metaData.CameraDir.has_value())
        std::cout << "  Dir:      " << FormatVector(metaData.CameraDir.value()) << std::endl;

    for (const auto& attrib : metaData.CustomStrings)
        std::cout << attrib.first << ": " << attrib.second << std::endl;
    for (const auto& attrib : metaData.CustomIntegers)
        std::cout << attrib.first << ": " << attrib.second << std::endl;
    for (const auto& attrib : metaData.CustomFloats)
        std::cout << attrib.first << ": " << attrib.second << std::endl;
    for (const auto& attrib : metaData.CustomVec2s)
        std::cout << attrib.first << ": " << FormatVector(attrib.second) << std::endl;
    for (const auto& attrib : metaData.CustomVec3s)
        std::cout << attrib.first << ": " << FormatVector(attrib.second) << std::endl;

    return true;
}
} // namespace IG