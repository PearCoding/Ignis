#include "CameraProxy.h"
#include "GlareOptions.h"
#include "Image.h"
#include "Logger.h"
#include "Runtime.h"
#include "Timer.h"
#include "config/Build.h"
#include "extra/GlareEvaluator.h"

using namespace IG;

int main(int argc, char** argv)
{
    GlareOptions cmd(argc, argv);
    if (cmd.ShouldExit)
        return EXIT_SUCCESS;

    RuntimeOptions opts;
    cmd.populate(opts);
    opts.EnableTonemapping = false;

    if (!cmd.Quiet)
        std::cout << Build::getCopyrightString() << std::endl;

    if (cmd.Input.empty()) {
        IG_LOG(L_ERROR) << "No input file given" << std::endl;
        return EXIT_FAILURE;
    }

    ImageMetaData metaData;
    Image image = Image::load(cmd.Input, &metaData);
    if (!image.isValid()) {
        IG_LOG(L_ERROR) << "Given input image is invalid" << std::endl;
        return EXIT_FAILURE;
    }
    if (image.channels != 3)
        image = image.castTo(3);

    std::unique_ptr<Runtime> runtime;
    std::shared_ptr<GlareEvaluator> glare;
    try {
        runtime = std::make_unique<Runtime>(opts);
        glare   = runtime->createGlareEvaluator();
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    const std::optional<Vector3f> dir = cmd.Dir.has_value() ? cmd.DirVector() : metaData.CameraDir;
    const std::optional<Vector3f> up  = cmd.Up.has_value() ? cmd.UpVector() : metaData.CameraUp;

    if (dir.has_value())
        runtime->setParameter("__camera_dir", *dir);
    if (up.has_value())
        runtime->setParameter("__camera_up", *up);

    glare->setUserData(image.pixels.get(), image.width, image.height, true);
    glare->setMultiplier(cmd.Multiplier);
    if (cmd.VerticalIlluminance.has_value())
        glare->setVerticalIlluminance(*cmd.VerticalIlluminance);

    const auto result = glare->run();

    glare.reset();
    runtime.reset();

    if (!result) {
        IG_LOG(L_ERROR) << "Could not compute glare information" << std::endl;
        return EXIT_FAILURE;
    }

    if (result->SourceOmega <= 0)
        IG_LOG(L_WARNING) << "No glare source detected. Metrics might be invalid" << std::endl;
    else if (result->SourceLuminance <= 0)
        IG_LOG(L_WARNING) << "Source luminance is 0. Metrics might be invalid" << std::endl;

    constexpr int SW = 8;
    std::cout << "DGP:    " << std::setw(SW) << result->DGP << std::endl
              << "DGI:    " << std::setw(SW) << result->DGI << std::endl
              << "DGImod: " << std::setw(SW) << result->DGImod << std::endl
              << "DGR:    " << std::setw(SW) << result->DGR << std::endl
              << "VCP:    " << std::setw(SW) << result->VCP << std::endl
              << "UGR:    " << std::setw(SW) << result->UGR << std::endl
              << "UGRexp: " << std::setw(SW) << result->UGRexp << std::endl
              << "UGP:    " << std::setw(SW) << result->UGP << std::endl
              << std::endl
              << "eV: " << std::setw(SW) << result->VerticalIlluminance << std::endl
              << "SL: " << std::setw(SW) << result->SourceLuminance << std::endl
              << "BL: " << std::setw(SW) << result->BackgroundLuminance << std::endl
              << "TL: " << std::setw(SW) << result->TotalLuminance << std::endl
              << "SO: " << std::setw(SW) << result->SourceOmega << std::endl
              << "TO: " << std::setw(SW) << result->TotalOmega << std::endl;

    return EXIT_SUCCESS;
}
