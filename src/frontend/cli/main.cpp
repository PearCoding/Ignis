#include "CameraProxy.h"
#include "IO.h"
#include "Logger.h"
#include "ProgramOptions.h"
#include "Runtime.h"
#include "StatusObserver.h"
#include "Timer.h"
#include "config/Build.h"

#include <fstream>

using namespace IG;

struct SectionTimer {
    Timer timer;
    size_t duration_ms = 0;

    inline void start() { timer.start(); }
    inline void stop() { duration_ms += timer.stopMS(); }
};

static std::string beautiful_time(uint64 ms)
{
    if (ms == 0)
        return "0ms";

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
    opts.EnableTonemapping = false;

    if (!cmd.Quiet)
        std::cout << Build::getCopyrightString() << std::endl;

    if (cmd.InputScene.empty()) {
        IG_LOG(L_ERROR) << "No input file given" << std::endl;
        return EXIT_FAILURE;
    }

    if (cmd.RenderTime.value_or(0) <= 0 && cmd.SPP.value_or(0) <= 0) {
        IG_LOG(L_ERROR) << "No valid spp count or render time given" << std::endl;
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

    if (opts.AcquireStats)
        runtime->recordStatistics(true); // CLI is always on

    runtime->mergeParametersFrom(cmd.UserEntries);
    timer_loading.stop();

    auto orientation = runtime->initialCameraOrientation();
    orientation.Eye  = cmd.EyeVector().value_or(orientation.Eye);
    orientation.Dir  = cmd.DirVector().value_or(orientation.Dir);
    orientation.Up   = cmd.UpVector().value_or(orientation.Up);
    runtime->setCameraOrientationParameter(orientation);

    const size_t SPI          = runtime->samplesPerIteration();
    const size_t desired_iter = static_cast<size_t>(std::ceil(cmd.SPP.value_or(0) / (float)SPI));

    if (cmd.SPP.has_value() && (cmd.SPP.value() % SPI) != 0)
        IG_LOG(L_WARNING) << "Given spp " << cmd.SPP.value() << " is not a multiple of the spi " << SPI << ". Using spp " << desired_iter * SPI << " instead" << std::endl;

    StatusObserver observer(!cmd.NoColor, 2, desired_iter * SPI /* Approx */, cmd.RenderTime.value_or(0));
    observer.begin();

    IG_LOG(L_INFO) << "Started rendering..." << std::endl;

    std::vector<double> samples_sec;

    SectionTimer timer_render;
    while (true) {
        if (!cmd.NoProgress)
            observer.update(runtime->currentSampleCount());

        auto ticks = std::chrono::high_resolution_clock::now();

        timer_render.start();
        runtime->step(samples_sec.size() != desired_iter - 1);
        timer_render.stop();

        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ticks).count();

        samples_sec.emplace_back(1000.0 * double(SPI * runtime->framebufferWidth() * runtime->framebufferHeight()) / double(elapsed_ms));
        if (desired_iter > 0 && samples_sec.size() == desired_iter)
            break;
        else if (cmd.RenderTime.has_value() && timer_render.duration_ms / 1000 > cmd.RenderTime.value())
            break;
    }

    if (!cmd.NoProgress)
        observer.end();

    SectionTimer timer_saving;
    timer_saving.start();
    if (!saveImageOutput(cmd.Output, *runtime, nullptr))
        IG_LOG(L_ERROR) << "Failed to save EXR file " << cmd.Output << std::endl;
    else
        IG_LOG(L_INFO) << "Result saved to " << cmd.Output << std::endl;
    timer_saving.stop();

    timer_all.stop();

    if (cmd.AcquireStats || cmd.AcquireFullStats) {
        auto& stream = IG_LOG_UNSAFE(L_INFO);
        if (runtime->target().isCPU()) {
            stream << runtime->statisticsForHost().dump(timer_all.duration_ms, runtime->currentIterationCount(), cmd.AcquireFullStats);
        } else {
            stream << runtime->statisticsForHost().dump(timer_all.duration_ms, runtime->currentIterationCount(), cmd.AcquireFullStats, " [Host]")
                   << runtime->statisticsForDevice().dump(timer_all.duration_ms, runtime->currentIterationCount(), cmd.AcquireFullStats, " [Device]");
        }

        stream << "  Iterations: " << runtime->currentIterationCount() << std::endl
               << "  SPP: " << runtime->currentSampleCount() << std::endl
               << "  SPI: " << SPI << std::endl
               << "  Time: " << beautiful_time(timer_all.duration_ms) << std::endl
               << "    Loading> " << beautiful_time(timer_loading.duration_ms) << std::endl
               << "    Render>  " << beautiful_time(timer_render.duration_ms) << std::endl
               << "    Saving>  " << beautiful_time(timer_saving.duration_ms) << std::endl;
    }

    if (!cmd.StatsFile.empty()) {
        IG_LOG(L_INFO) << "Writing profiling data to " << cmd.StatsFile << std::endl;
        runtime->statisticsForDevice().finalizeStream();
        std::ofstream out_json(cmd.StatsFile);
        out_json << runtime->statisticsForDevice().dumpAsJSON((float)timer_all.duration_ms);
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
