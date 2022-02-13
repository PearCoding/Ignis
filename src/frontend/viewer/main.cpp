#include "Camera.h"
#include "IO.h"

#ifdef WITH_UI
#include "UI.h"
#else
#include "StatusObserver.h"
#endif

#include "Logger.h"
#include "Runtime.h"
#include "config/Build.h"

#include "Timer.h"

#include <optional>

using namespace IG;

static inline void check_arg(int argc, char** argv, int arg, int n)
{
    if (arg + n >= argc)
        IG_LOG(L_ERROR) << "Option '" << argv[arg] << "' expects " << n << " arguments, got " << (argc - arg) << std::endl;
}

#ifdef WITH_UI
static const char* PROGRAM_NAME = "igview";
#else
static const char* PROGRAM_NAME = "igcli";
#endif

static inline void version()
{
    std::cout << PROGRAM_NAME << " " << Build::getVersionString() << std::endl;
}

static inline void usage()
{
    std::cout
#ifdef WITH_UI
        << PROGRAM_NAME << " - Ignis Viewer" << std::endl
#else
        << PROGRAM_NAME << " - Ignis Command Line Renderer" << std::endl
#endif
        << Build::getCopyrightString() << std::endl
        << "Usage: " << PROGRAM_NAME << " [options] file" << std::endl
        << "Available options:" << std::endl
        << "   -h      --help                 Shows this message" << std::endl
        << "           --version              Show version and exit" << std::endl
        << "   -q      --quiet                Do not print messages into console" << std::endl
        << "   -v      --verbose              Print detailed information" << std::endl
        << "           --no-color             Do not use decorations to make console output better" << std::endl
        << "           --width     pixels     Sets the viewport horizontal dimension (in pixels)" << std::endl
        << "           --height    pixels     Sets the viewport vertical dimension (in pixels)" << std::endl
        << "           --eye       x y z      Sets the position of the camera" << std::endl
        << "           --dir       x y z      Sets the direction vector of the camera" << std::endl
        << "           --up        x y z      Sets the up vector of the camera" << std::endl
        << "           --fov       degrees    Sets the horizontal field of view (in degrees)" << std::endl
        << "           --range     tmin tmax  Sets near and far clip range in world units" << std::endl
        << "           --camera    cam_type   Override camera type" << std::endl
        << "           --technique tech_type  Override technique/integrator type" << std::endl
        << "   -t      --target    target     Sets the target platform (default: autodetect CPU)" << std::endl
        << "   -d      --device    device     Sets the device to use on the selected platform (default: 0)" << std::endl
        << "           --cpu                  Use autodetected CPU target" << std::endl
        << "           --gpu                  Use autodetected GPU target" << std::endl
        << "           --debug                Same as --technique debug" << std::endl
        << "           --spp       spp        Enables benchmarking mode and sets the number of iterations based on the given spp" << std::endl
#ifdef WITH_UI
        << "           --spp-mode  spp_mode   Sets the current spp mode (default: fixed)" << std::endl
#endif
        << "           --stats                Acquire useful stats alongside rendering. Will be dumped at the end of the rendering session" << std::endl
        << "           --full-stats           Acquire all stats alongside rendering. Will be dumped at the end of the rendering session" << std::endl
        << "   -o      --output    image.exr  Writes the output image to a file" << std::endl
        << "           --dump-shader          Dump produced shaders to files in the current working directory" << std::endl
        << "           --dump-shader-full     Dump produced shaders with standard library to files in the current working directory" << std::endl
        << "Available targets:" << std::endl
        << "    generic, sse42, avx, avx2, avx512, asimd," << std::endl
        << "    nvvm, amdgpu" << std::endl
        << "Available cameras:" << std::endl
        << "    perspective, orthogonal, fishlens" << std::endl
        << "Available techniques:" << std::endl
        << "    path, debug, ao" << std::endl
#ifdef WITH_UI
        << "Available spp modes:" << std::endl
        << "    fixed      Finish after reaching the given spp. This will close the program after the desired spp has been reached" << std::endl
        << "    capped     Do not render any further than the given spp for the current view. This will freeze the FPS" << std::endl
        << "    continous  Rerender view after given spp is reached. This simulates real-time raytracing habbits. Can be flickering quite a lot!" << std::endl
#endif
        ;
}

struct SectionTimer {
    Timer timer;
    size_t duration_ms = 0;

    inline void start() { timer.start(); }
    inline void stop() { duration_ms += timer.stopMS(); }
};

enum class SPPMode {
    Fixed,
    Capped,
    Continous
};

int main(int argc, char** argv)
{
    if (argc <= 1) {
        usage();
        return EXIT_SUCCESS;
    }

    std::string in_file;
    std::string out_file;
    size_t desired_spp = 0;
    SPPMode spp_mode   = SPPMode::Fixed;
    std::optional<Vector3f> eye;
    std::optional<Vector3f> dir;
    std::optional<Vector3f> up;
    std::optional<float> fov;
    std::optional<Vector2f> trange;
    bool prettyConsole = true;
    bool quiet         = false;

    RuntimeOptions opts;
    bool all_stats = false;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "--width")) {
                check_arg(argc, argv, i, 1);
                opts.OverrideFilmSize.first = strtoul(argv[i + 1], nullptr, 10);
                ++i;
            } else if (!strcmp(argv[i], "--height")) {
                check_arg(argc, argv, i, 1);
                opts.OverrideFilmSize.second = strtoul(argv[i + 1], nullptr, 10);
                ++i;
            } else if (!strcmp(argv[i], "--eye")) {
                check_arg(argc, argv, i, 3);
                eye = { Vector3f(strtof(argv[i + 1], nullptr), strtof(argv[i + 2], nullptr), strtof(argv[i + 3], nullptr)) };
                i += 3;
            } else if (!strcmp(argv[i], "--dir")) {
                check_arg(argc, argv, i, 3);
                dir = { Vector3f(strtof(argv[i + 1], nullptr), strtof(argv[i + 2], nullptr), strtof(argv[i + 3], nullptr)) };
                i += 3;
            } else if (!strcmp(argv[i], "--up")) {
                check_arg(argc, argv, i, 3);
                up = { Vector3f(strtof(argv[i + 1], nullptr), strtof(argv[i + 2], nullptr), strtof(argv[i + 3], nullptr)) };
                i += 3;
            } else if (!strcmp(argv[i], "--range")) {
                check_arg(argc, argv, i, 2);
                trange = { Vector2f(strtof(argv[i + 1], nullptr), strtof(argv[i + 2], nullptr)) };
                i += 2;
            } else if (!strcmp(argv[i], "--fov")) {
                check_arg(argc, argv, i, 1);
                fov = strtof(argv[i + 1], nullptr);
                ++i;
            } else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--target")) {
                check_arg(argc, argv, i, 1);
                ++i;
                if (!strcmp(argv[i], "sse42"))
                    opts.DesiredTarget = Target::SSE42;
                else if (!strcmp(argv[i], "avx"))
                    opts.DesiredTarget = Target::AVX;
                else if (!strcmp(argv[i], "avx2"))
                    opts.DesiredTarget = Target::AVX2;
                else if (!strcmp(argv[i], "avx512"))
                    opts.DesiredTarget = Target::AVX512;
                else if (!strcmp(argv[i], "asimd"))
                    opts.DesiredTarget = Target::ASIMD;
                else if (!strcmp(argv[i], "nvvm"))
                    opts.DesiredTarget = Target::NVVM;
                else if (!strcmp(argv[i], "amdgpu"))
                    opts.DesiredTarget = Target::AMDGPU;
                else if (!strcmp(argv[i], "generic"))
                    opts.DesiredTarget = Target::GENERIC;
                else {
                    IG_LOG(L_ERROR) << "Unknown target '" << argv[i] << "'. Aborting." << std::endl;
                    return EXIT_FAILURE;
                }
            } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--device")) {
                check_arg(argc, argv, i, 1);
                ++i;
                opts.Device = strtoul(argv[i], NULL, 10);
            } else if (!strcmp(argv[i], "--cpu")) {
                opts.RecommendCPU = true;
                opts.RecommendGPU = false;
            } else if (!strcmp(argv[i], "--gpu")) {
                opts.RecommendCPU = false;
                opts.RecommendGPU = true;
            } else if (!strcmp(argv[i], "--spp")) {
                check_arg(argc, argv, i, 1);
                desired_spp = (size_t)strtoul(argv[++i], nullptr, 10);
            } else if (!strcmp(argv[i], "--spi")) {
                check_arg(argc, argv, i, 1);
                opts.SPI = (size_t)strtoul(argv[++i], nullptr, 10);
#ifdef WITH_UI
            } else if (!strcmp(argv[i], "--spp-mode")) {
                check_arg(argc, argv, i, 1);
                ++i;
                if (!strcmp(argv[i], "fixed"))
                    spp_mode = SPPMode::Fixed;
                else if (!strcmp(argv[i], "capped"))
                    spp_mode = SPPMode::Capped;
                else if (!strcmp(argv[i], "continous"))
                    spp_mode = SPPMode::Continous;
                else {
                    IG_LOG(L_ERROR) << "Unknown spp mode '" << argv[i] << "'. Aborting." << std::endl;
                    return EXIT_FAILURE;
                }
#endif
            } else if (!strcmp(argv[i], "-o")) {
                check_arg(argc, argv, i, 1);
                ++i;
                out_file = argv[i];
            } else if (!strcmp(argv[i], "--camera")) {
                check_arg(argc, argv, i, 1);
                ++i;
                opts.OverrideCamera = argv[i];
            } else if (!strcmp(argv[i], "--technique")) {
                check_arg(argc, argv, i, 1);
                ++i;
                opts.OverrideTechnique = argv[i];
            } else if (!strcmp(argv[i], "--debug")) {
                opts.OverrideTechnique = "debug";
            } else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet")) {
                quiet = true;
                IG_LOGGER.setQuiet(true);
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                IG_LOGGER.setVerbosity(L_DEBUG);
            } else if (!strcmp(argv[i], "--no-color")) {
                prettyConsole = false;
                IG_LOGGER.enableAnsiTerminal(false);
            } else if (!strcmp(argv[i], "--dump-shader")) {
                opts.DumpShader = true;
            } else if (!strcmp(argv[i], "--dump-shader-full")) {
                opts.DumpShaderFull = true;
            } else if (!strcmp(argv[i], "--stats")) {
                opts.AcquireStats = true;
            } else if (!strcmp(argv[i], "--full-stats")) {
                opts.AcquireStats = true;
                all_stats         = true;
            } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                usage();
                return EXIT_SUCCESS;
            } else if (!strcmp(argv[i], "--version")) {
                version();
                return EXIT_SUCCESS;
            } else {
                IG_LOG(L_ERROR) << "Unknown option '" << argv[i] << "'" << std::endl;
                return EXIT_FAILURE;
            }
        } else {
            if (in_file.empty()) {
                in_file = argv[i];
            } else {
                IG_LOG(L_ERROR) << "Unexpected argument '" << argv[i] << "'" << std::endl;
                return EXIT_FAILURE;
            }
        }
    }

    if (!quiet)
        std::cout << Build::getCopyrightString() << std::endl;

    if (in_file.empty()) {
        IG_LOG(L_ERROR) << "No input file given" << std::endl;
        return EXIT_FAILURE;
    }

#ifdef WITH_UI
    if (spp_mode != SPPMode::Fixed && desired_spp <= 0) {
        IG_LOG(L_ERROR) << "No valid spp count given. Required by the capped or continous spp mode" << std::endl;
        return EXIT_FAILURE;
    }
#else
    if (desired_spp <= 0) {
        IG_LOG(L_ERROR) << "No valid spp count given" << std::endl;
        return EXIT_FAILURE;
    }
    if (out_file.empty()) {
        IG_LOG(L_ERROR) << "No output file given" << std::endl;
        return EXIT_FAILURE;
    }
#endif

    SectionTimer timer_all;
    SectionTimer timer_loading;
    timer_all.start();
    timer_loading.start();

    std::unique_ptr<Runtime> runtime;
    try {
        runtime = std::make_unique<Runtime>(in_file, opts);
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    timer_loading.stop();

    const auto def  = runtime->loadedRenderSettings();
    const auto clip = trange.value_or(Vector2f(def.TMin, def.TMax));
    Camera camera(eye.value_or(def.CameraEye), dir.value_or(def.CameraDir), up.value_or(def.CameraUp),
                  fov.value_or(def.FOV), (float)def.FilmWidth / (float)def.FilmHeight,
                  clip(0), clip(1));
    runtime->setup();

    const size_t SPI          = runtime->samplesPerIteration();
    const size_t desired_iter = static_cast<size_t>(std::ceil(desired_spp / (float)SPI));

    if (desired_spp > 0 && (desired_spp % SPI) != 0)
        IG_LOG(L_WARNING) << "Given spp " << desired_spp << " is not a multiple of the spi " << SPI << ". Using spp " << desired_iter * SPI << " instead" << std::endl;

#ifdef WITH_UI
    IG_UNUSED(prettyConsole);

    std::unique_ptr<UI> ui;
    try {
        size_t aov_count = runtime->aovs().size() + 1;
        std::vector<const float*> aovs(aov_count);
        std::vector<std::string> aov_names(aov_count);
        for (size_t i = 0; i < aov_count; ++i) {
            aovs[i] = runtime->getFramebuffer(i);
            IG_ASSERT(aovs[i], "Expected framebuffer entry for AOV to be not NULL");
            if (i == 0)
                aov_names[0] = "Color";
            else
                aov_names[i] = runtime->aovs()[i - 1];
        }
        ui = std::make_unique<UI>(def.FilmWidth, def.FilmHeight, aovs, aov_names, runtime->isDebug());

        // Setup initial travelspeed
        BoundingBox bbox = runtime->sceneBoundingBox();
        bbox.extend(camera.Eye);
        ui->setTravelSpeed(bbox.diameter().maxCoeff() / 50);
    } catch (...) {
        return EXIT_FAILURE;
    }

    runtime->setDebugMode(ui->currentDebugMode());
#else
    StatusObserver observer(prettyConsole, 2, desired_iter * SPI);
    observer.begin();
#endif

    IG_LOG(L_INFO) << "Started rendering..." << std::endl;

    bool running    = true;
    bool done       = false;
    uint64_t timing = 0;
    uint32_t frames = 0;
    uint32_t iter   = 0;
    std::vector<double> samples_sec;

#ifdef WITH_UI
    SectionTimer timer_input;
    SectionTimer timer_ui;
#endif

    SectionTimer timer_render;
    while (!done) {
#ifdef WITH_UI
        bool prevRun = running;

        timer_input.start();
        done = ui->handleInput(iter, running, camera);
        timer_input.stop();

        if (runtime->currentDebugMode() != ui->currentDebugMode()) {
            runtime->setDebugMode(ui->currentDebugMode());
            iter = 0;
        }
#else
        observer.update(iter * SPI);
#endif

        if (running) {
            if (spp_mode != SPPMode::Capped || iter < desired_iter) {
                if (spp_mode == SPPMode::Continous && iter >= desired_iter) {
                    iter = 0;
                }

                if (iter == 0)
                    runtime->reset();

                auto ticks = std::chrono::high_resolution_clock::now();

                timer_render.start();
                runtime->step(camera);
                timer_render.stop();

                iter++;
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ticks).count();

                if (spp_mode == SPPMode::Fixed && desired_iter != 0) {
                    samples_sec.emplace_back(1000.0 * double(SPI * def.FilmWidth * def.FilmHeight) / double(elapsed_ms));
                    if (samples_sec.size() == desired_iter)
                        break;
                }

                frames++;
                timing += elapsed_ms;
                if (frames > 10 || timing >= 2000) {
#ifdef WITH_UI
                    const double frames_sec = double(frames) * 1000.0 / double(timing);

                    std::ostringstream os;
                    os << "Ignis [" << frames_sec << " FPS, "
                       << frames_sec * SPI << " SPS, "
                       << iter * SPI << " "
                       << "sample" << (iter * SPI > 1 ? "s" : "") << "]";
                    ui->setTitle(os.str().c_str());
#endif
                    frames = 0;
                    timing = 0;
                }
            } else {
#ifdef WITH_UI
                std::ostringstream os;
                os << "Ignis [Capped, "
                   << iter * SPI << " "
                   << "sample" << (iter * SPI > 1 ? "s" : "") << "]";
                ui->setTitle(os.str().c_str());
#endif
            }
        } else {
            frames++;

#ifdef WITH_UI
            if (prevRun != running || frames > 100) {
                std::ostringstream os;
                os << "Ignis [Paused, "
                   << iter * SPI << " "
                   << "sample" << (iter * SPI > 1 ? "s" : "") << "]";
                ui->setTitle(os.str().c_str());
                frames = 0;
                timing = 0;
            }
#endif
        }

#ifdef WITH_UI
        timer_ui.start();
        ui->update(runtime->currentIterationCountForFramebuffer(), SPI);
        timer_ui.stop();
#endif
    }

#ifdef WITH_UI
    ui.reset();
#else
    observer.end();
#endif

    SectionTimer timer_saving;
    timer_saving.start();
    if (!out_file.empty()) {
        if (!saveImageOutput(out_file, *runtime))
            IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file << "'" << std::endl;
        else
            IG_LOG(L_INFO) << "Result saved to '" << out_file << "'" << std::endl;
    }
    timer_saving.stop();

    timer_all.stop();

    auto stats = runtime->getStatistics();
    if (stats) {
        IG_LOG(L_INFO)
            << stats->dump(iter, all_stats)
            << "  Iterations: " << iter << std::endl
            << "  SPP: " << iter * SPI << std::endl
            << "  SPI: " << SPI << std::endl
            << "  Time: " << timer_all.duration_ms << "ms" << std::endl
            << "    Loading> " << timer_loading.duration_ms << "ms" << std::endl
#ifdef WITH_UI
            << "    Input>   " << timer_input.duration_ms << "ms" << std::endl
            << "    UI>      " << timer_ui.duration_ms << "ms" << std::endl
#endif
            << "    Render>  " << timer_render.duration_ms << "ms" << std::endl
            << "    Saving>  " << timer_saving.duration_ms << "ms" << std::endl;
    }

    runtime.reset();

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
