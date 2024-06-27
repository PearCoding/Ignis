#include "CameraProxy.h"
#include "Context.h"
#include "IO.h"
#include "Logger.h"
#include "ProgramOptions.h"
#include "Runtime.h"
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

int main(int argc, char** argv)
{
    ProgramOptions cmd(argc, argv, ApplicationType::View, "Interactive viewer");
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

    if (cmd.SPPMode != SPPMode::Fixed && !cmd.SPP.has_value()) {
        IG_LOG(L_ERROR) << "No valid spp count given. Required by the capped or continuous spp mode" << std::endl;
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
    timer_loading.stop();

    const auto def = runtime->initialCameraOrientation();
    CameraProxy camera(cmd.EyeVector().value_or(def.Eye), cmd.DirVector().value_or(def.Dir), cmd.UpVector().value_or(def.Up));
    runtime->setCameraOrientation(camera.asOrientation());

    const size_t SPI          = runtime->samplesPerIteration();
    const size_t desired_iter = static_cast<size_t>(std::ceil(cmd.SPP.value_or(0) / (float)SPI));

    if (cmd.SPP.has_value() && (cmd.SPP.value() % SPI) != 0)
        IG_LOG(L_WARNING) << "Given spp " << cmd.SPP.value() << " is not a multiple of the spi " << SPI << ". Using spp " << desired_iter * SPI << " instead" << std::endl;

    std::unique_ptr<Context> ui;
    try {
        ui = std::make_unique<Context>(cmd.SPPMode, runtime.get(), runtime->technique() == "debug", cmd.DPI.value_or(-1));
    } catch (...) {
        return EXIT_FAILURE;
    }

    // Setup initial travel-speed
    BoundingBox bbox = runtime->sceneBoundingBox();
    bbox.extend(camera.Eye);
    ui->setTravelSpeed(std::max(1e-4f, bbox.diameter().maxCoeff() / 50));

    auto lastDebugMode = ui->currentDebugMode();

    IG_LOG(L_INFO) << "Started rendering..." << std::endl;

    bool running     = true;
    bool done        = false;
    uint64_t timing  = 0;
    uint32_t frames  = 0;
    size_t totalIter = 0; // Total number of iterations, without ever reseting
    std::vector<double> samples_stats;

    SectionTimer timer_input;
    SectionTimer timer_ui;
    SectionTimer timer_render;
    while (!done) {
        timer_input.start();
        const auto input_result = ui->handleInput(camera);
        switch (input_result) {
        case Context::InputResult::Quit:
            done = true;
            break;
        case Context::InputResult::Resume:
            running = true;
            break;
        case Context::InputResult::Pause:
            running = false;
            break;
        case Context::InputResult::Reset:
            runtime->setCameraOrientation(camera.asOrientation());
            runtime->reset();
            break;
        default:
            break;
        }

        if (lastDebugMode != ui->currentDebugMode()) {
            runtime->setParameter("__debug_mode", (int)ui->currentDebugMode());
            runtime->reset();
            lastDebugMode = ui->currentDebugMode();
        }
        timer_input.stop();

        if (running) {
            if (cmd.SPPMode != SPPMode::Capped || runtime->currentIterationCount() < desired_iter) {
                if (cmd.SPPMode == SPPMode::Continuous && runtime->currentIterationCount() >= desired_iter) {
                    runtime->reset();
                    runtime->incFrameCount(); // Not affected by reset
                }

                auto ticks = std::chrono::high_resolution_clock::now();

                timer_render.start();
                runtime->step();
                ++totalIter;
                timer_render.stop();

                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ticks).count();

                if (cmd.SPPMode == SPPMode::Fixed && desired_iter != 0) {
                    samples_stats.emplace_back(1000.0 * double(SPI * runtime->framebufferWidth() * runtime->framebufferHeight()) / double(elapsed_ms));
                    if (samples_stats.size() == desired_iter)
                        break;
                }

                frames++;
                timing += elapsed_ms;
                if (frames > 10 || timing >= 2000) {
                    const double frames_sec  = double(frames) * 1000.0 / double(timing);
                    const double samples_sec = (runtime->currentIterationCount() == 0) ? 0.0 : frames_sec * runtime->currentSampleCount() / (float)runtime->currentIterationCount();

                    std::ostringstream os;
                    os << "Ignis [" << frames_sec << " FPS, "
                       << samples_sec << " SPS, "
                       << runtime->currentSampleCount() << " "
                       << "sample" << (runtime->currentSampleCount() > 1 ? "s" : "") << "]";
                    ui->setTitle(os.str().c_str());

                    frames = 0;
                    timing = 0;
                }
            } else {
                std::ostringstream os;
                os << "Ignis [Capped, "
                   << runtime->currentSampleCount() << " "
                   << "sample" << (runtime->currentSampleCount() > 1 ? "s" : "") << "]";
                ui->setTitle(os.str().c_str());
            }
        } else {
            frames++;

            if (input_result == Context::InputResult::Pause || frames > 100) {
                std::ostringstream os;
                os << "Ignis [Paused, "
                   << runtime->currentSampleCount() << " "
                   << "sample" << (runtime->currentSampleCount() > 1 ? "s" : "") << "]";
                ui->setTitle(os.str().c_str());
                frames = 0;
                timing = 0;
            }
        }

        timer_ui.start();
        switch (ui->update()) {
        case Context::UpdateResult::Reset:
            runtime->reset();
            break;
        default:
            break;
        }
        timer_ui.stop();
    }

    ui.reset();

    SectionTimer timer_saving;
    timer_saving.start();
    if (!cmd.Output.empty()) {
        CameraOrientation orientation;
        orientation.Eye = camera.Eye;
        orientation.Up  = camera.Up;
        orientation.Dir = camera.Direction;
        if (!saveImageOutput(cmd.Output, *runtime, &orientation))
            IG_LOG(L_ERROR) << "Failed to save EXR file " << cmd.Output << std::endl;
        else
            IG_LOG(L_INFO) << "Result saved to " << cmd.Output << std::endl;
    }
    timer_saving.stop();

    timer_all.stop();

    auto stats = runtime->statistics();
    if (stats) {
        IG_LOG(L_INFO)
            << stats->dump(timer_all.duration_ms, totalIter, cmd.AcquireFullStats)
            << "  Iterations: " << runtime->currentIterationCount() << " (total: " << totalIter << ")" << std::endl
            << "  SPP: " << runtime->currentSampleCount() << std::endl
            << "  SPI: " << SPI << std::endl
            << "  Time: " << beautiful_time(timer_all.duration_ms) << std::endl
            << "    Loading> " << beautiful_time(timer_loading.duration_ms) << std::endl
            << "    Input>   " << beautiful_time(timer_input.duration_ms) << std::endl
            << "    UI>      " << beautiful_time(timer_ui.duration_ms) << std::endl
            << "    Render>  " << beautiful_time(timer_render.duration_ms) << std::endl
            << "    Saving>  " << beautiful_time(timer_saving.duration_ms) << std::endl;
    }

    runtime.reset();

    if (!samples_stats.empty())
        IG_LOG(L_INFO) << "Rendering took " << beautiful_time(timer_all.duration_ms) << std::endl;

    if (!samples_stats.empty()) {
        auto inv = 1.0e-6;
        std::sort(samples_stats.begin(), samples_stats.end());
        IG_LOG(L_INFO) << "# " << samples_stats.front() * inv
                       << "/" << samples_stats[samples_stats.size() / 2] * inv
                       << "/" << samples_stats.back() * inv
                       << " (min/med/max Msamples/s)" << std::endl;
    }

    return EXIT_SUCCESS;
}
