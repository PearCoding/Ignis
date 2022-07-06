#include "Camera.h"
#include "IO.h"
#include "Logger.h"
#include "ProgramOptions.h"
#include "Runtime.h"
#include "StatusObserver.h"
#include "Timer.h"
#include "config/Build.h"

using namespace IG;

struct SectionTimer {
    Timer timer;
    size_t duration_ms = 0;

    inline void start() { timer.start(); }
    inline void stop() { duration_ms += timer.stopMS(); }
};

static std::string beautiful_time(uint64 ms)
{
    uint64_t pms = ms % 1000;
    ms /= 1000;
    uint64_t ps = ms % 60;
    ms /= 60;
    uint64_t pm = ms % 60;
    ms /= 60;
    uint64_t ph = ms % 24;
    ms /= 24;
    uint64_t pd = ms;

    std::stringstream stream;
    if (pd > 0)
        stream << pd << "d ";

    if (ph > 0)
        stream << ph << "h ";

    if (pm > 0)
        stream << pm << "m ";

    if (pm > 0 || ps > 0)
        stream << ps << "s ";

    if (pms > 0)
        stream << pms << "ms";

    return stream.str();
}

int main(int argc, char** argv)
{
    ProgramOptions cmd(argc, argv, ApplicationType::CLI, "Command Line Interface");
    if (cmd.ShouldExit)
        return EXIT_SUCCESS;

    RuntimeOptions opts;
    cmd.populate(opts);

    if (!cmd.Quiet)
        std::cout << Build::getCopyrightString() << std::endl;

    if (cmd.InputScene.empty()) {
        IG_LOG(L_ERROR) << "No input file given" << std::endl;
        return EXIT_FAILURE;
    }

    if (cmd.SPP.value_or(0) <= 0) {
        IG_LOG(L_ERROR) << "No valid spp count given" << std::endl;
        return EXIT_FAILURE;
    }

    if (cmd.Output.empty()) {
        IG_LOG(L_ERROR) << "No output file given" << std::endl;
        return EXIT_FAILURE;
    }

    SectionTimer timer_all;
    SectionTimer timer_loading;
    timer_all.start();
    timer_loading.start();

    std::unique_ptr<Runtime> runtime;
    try {
        runtime = std::make_unique<Runtime>(opts);
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    if (!runtime->loadFromFile(cmd.InputScene)) {
        IG_LOG(L_ERROR) << "Could not load " << cmd.InputScene << std::endl;
        return EXIT_FAILURE;
    }

    timer_loading.stop();

    const auto def = runtime->initialCameraOrientation();
    runtime->setParameter("__camera_eye", cmd.EyeVector().value_or(def.Eye));
    runtime->setParameter("__camera_dir", cmd.DirVector().value_or(def.Dir));
    runtime->setParameter("__camera_up", cmd.UpVector().value_or(def.Up));

    const size_t SPI          = runtime->samplesPerIteration();
    const size_t desired_iter = static_cast<size_t>(std::ceil(cmd.SPP.value_or(0) / (float)SPI));

    if (cmd.SPP.has_value() && (cmd.SPP.value() % SPI) != 0)
        IG_LOG(L_WARNING) << "Given spp " << cmd.SPP.value() << " is not a multiple of the spi " << SPI << ". Using spp " << desired_iter * SPI << " instead" << std::endl;

    StatusObserver observer(!cmd.NoColor, 2, desired_iter * SPI /* Approx */);
    observer.begin();

    IG_LOG(L_INFO) << "Started rendering..." << std::endl;

    std::vector<double> samples_sec;

    // std::vector<uint32_t> filter_Buffer;
    // std::vector<uint32_t> Buffer;
    

    // auto width = runtime->framebufferWidth();
    // auto height = runtime->framebufferHeight();
    // Buffer.resize(width * height);
    // filter_Buffer.resize(width * height);
    // std::cout << Buffer.size();
    // IG_LOG(L_INFO) << "Size " << Buffer.size() << std::endl;
    
    // init framebuffer variable
    // uint32* filter_buf = filter_Buffer.data();
    
    SectionTimer timer_render;
    while (true) {
        if (!cmd.NoProgress)
            observer.update(runtime->currentSampleCount());

        auto ticks = std::chrono::high_resolution_clock::now();

        timer_render.start();
        runtime->step();
        //Add filter here 100 percent sure hehehehe
        // runtime->getFramebuffer()
        runtime->filter();

        // SDL_UpdateTexture(Texture, nullptr, buf, static_cast<int>(Width * sizeof(uint32_t)));

        timer_render.stop();
        

        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ticks).count();

        samples_sec.emplace_back(1000.0 * double(SPI * runtime->framebufferWidth() * runtime->framebufferHeight()) / double(elapsed_ms));
        if (samples_sec.size() == desired_iter)
            break;
    }

    if (!cmd.NoProgress)
        observer.end();

    SectionTimer timer_saving;
    timer_saving.start();
    if (!saveImageOutput(cmd.Output, *runtime))
        IG_LOG(L_ERROR) << "Failed to save EXR file " << cmd.Output << std::endl;
    else
        IG_LOG(L_INFO) << "Result saved to " << cmd.Output << std::endl;
    timer_saving.stop();

    timer_all.stop();

    auto stats = runtime->getStatistics();
    if (stats) {
        IG_LOG(L_INFO)
            << stats->dump(timer_all.duration_ms, runtime->currentIterationCount(), cmd.AcquireFullStats)
            << "  Iterations: " << runtime->currentIterationCount() << std::endl
            << "  SPP: " << runtime->currentSampleCount() << std::endl
            << "  SPI: " << SPI << std::endl
            << "  Time: " << beautiful_time(timer_all.duration_ms) << std::endl
            << "    Loading> " << beautiful_time(timer_loading.duration_ms) << std::endl
            << "    Render>  " << beautiful_time(timer_render.duration_ms) << std::endl
            << "    Saving>  " << beautiful_time(timer_saving.duration_ms) << std::endl;
    }

    runtime.reset();

    IG_LOG(L_INFO) << "Rendering took " << beautiful_time(timer_all.duration_ms) << std::endl;

    if (!samples_sec.empty()) {
        auto inv = 1.0e-6;
        std::sort(samples_sec.begin(), samples_sec.end());
        IG_LOG(L_INFO) << "# " << samples_sec.front() * inv
                       << "/" << samples_sec[samples_sec.size() / 2] * inv
                       << "/" << samples_sec.back() * inv
                       << " (min/med/max Msamples/s)" << std::endl;
    }

    return EXIT_SUCCESS;
}
