#include "CameraProxy.h"
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

static inline Path append_iteration(const Path& p, size_t iteration)
{
    const std::string suffix = "_iter_" + std::to_string(iteration);
    return Path(p).replace_filename(p.stem().generic_string() + suffix + p.extension().generic_string());
}

static inline Path append_seconds(const Path& p, size_t seconds)
{
    const std::string suffix = "_time_" + std::to_string(seconds);
    return Path(p).replace_filename(p.stem().generic_string() + suffix + p.extension().generic_string());
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

    runtime->mergeParametersFrom(cmd.UserEntries);

    if (!cmd.ContinueImage.empty())
        runtime->loadPreviousFramebuffer(cmd.ContinueImage);

    timer_loading.stop();

    auto orientation = runtime->initialCameraOrientation();
    orientation.Eye  = cmd.EyeVector().value_or(orientation.Eye);
    orientation.Dir  = cmd.DirVector().value_or(orientation.Dir);
    orientation.Up   = cmd.UpVector().value_or(orientation.Up);
    runtime->setCameraOrientation(orientation);

    const size_t SPI          = runtime->samplesPerIteration();
    const size_t desired_iter = static_cast<size_t>(std::ceil(cmd.SPP.value_or(0) / (float)SPI));

    if (cmd.SPP.has_value() && (cmd.SPP.value() % SPI) != 0)
        IG_LOG(L_WARNING) << "Given spp " << cmd.SPP.value() << " is not a multiple of the spi " << SPI << ". Using spp " << desired_iter * SPI << " instead" << std::endl;

    if (runtime->currentSampleCount() >= (size_t)cmd.SPP.value_or(std::numeric_limits<int>::max()))
        IG_LOG(L_WARNING) << "Requested spp already satisfied. No need to continue." << std::endl;

    StatusObserver observer(!cmd.NoColor, 2, desired_iter * SPI /* Approx */, cmd.RenderTime.value_or(0));
    observer.begin();

    IG_LOG(L_INFO) << "Started rendering..." << std::endl;

    std::vector<double> samples_sec;

    auto progress_time = std::chrono::high_resolution_clock::now();
    SectionTimer timer_render;
    while (runtime->currentSampleCount() < (size_t)cmd.SPP.value_or(std::numeric_limits<int>::max())) {
        if (!cmd.NoProgress)
            observer.update(runtime->currentSampleCount());

        auto ticks = std::chrono::high_resolution_clock::now();

        timer_render.start();
        runtime->step(samples_sec.size() != desired_iter - 1);
        timer_render.stop();

        const auto elapsed_ms     = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ticks).count();
        const auto elapsed_prog_s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - progress_time).count();

        if (cmd.StatusEveryIteration.has_value() && (runtime->currentIterationCount() % *cmd.StatusEveryIteration) == 0) {
            const Path tmp_output = append_iteration(cmd.Output, runtime->currentIterationCount());
            if (!runtime->saveFramebuffer(tmp_output))
                IG_LOG(L_ERROR) << "Failed to save EXR file " << tmp_output << std::endl;
        }

        if (cmd.StatusEverySeconds.has_value() && (size_t)elapsed_prog_s > *cmd.StatusEverySeconds) {
            progress_time         = std::chrono::high_resolution_clock::now();
            const Path tmp_output = append_seconds(cmd.Output, (size_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - runtime->renderStartTime()).count());
            if (!runtime->saveFramebuffer(tmp_output))
                IG_LOG(L_ERROR) << "Failed to save EXR file " << tmp_output << std::endl;
        }

        samples_sec.emplace_back(1000.0 * double(SPI * runtime->framebufferWidth() * runtime->framebufferHeight()) / double(elapsed_ms));
        if (cmd.RenderTime.has_value() && timer_render.duration_ms / 1000 > cmd.RenderTime.value())
            break;
    }

    if (!cmd.NoProgress)
        observer.end();

    if (samples_sec.empty()) {
        IG_LOG(L_WARNING) << "Nothing rendered." << std::endl;
        return EXIT_SUCCESS;
    }

    SectionTimer timer_saving;
    timer_saving.start();
    if (!runtime->saveFramebuffer(cmd.Output))
        IG_LOG(L_ERROR) << "Failed to save EXR file " << cmd.Output << std::endl;
    else
        IG_LOG(L_INFO) << "Result saved to " << cmd.Output << std::endl;
    timer_saving.stop();

    timer_all.stop();

    if (cmd.AcquireStats || cmd.AcquireFullStats) {
        const auto& stats = runtime->statistics();
        IG_LOG(L_INFO)
            << stats.dump(timer_all.duration_ms, runtime->currentIterationCount(), cmd.AcquireFullStats)
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
