#include <filesystem>
#include <iostream>

#include "Image.h"
#include "ImageIO.h"
#include "Logger.h"

using namespace IG;

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cout << "Expected exrinfo INPUT" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string input = argv[1];

    try {
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

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}