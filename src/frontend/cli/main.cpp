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

static const char* PROGRAM_NAME = "igcli";

static inline void version()
{
    std::cout << PROGRAM_NAME << " " << Build::getVersionString() << std::endl;
}

static inline void usage()
{
    std::cout << PROGRAM_NAME << " - Ignis Command Line Renderer" << std::endl
              << Build::getCopyrightString() << std::endl
              << "Usage: " << PROGRAM_NAME << " [options] file" << std::endl
              << "Available options:" << std::endl
              << "   -h      --help                 Shows this message" << std::endl
              << "           --version              Show version and exit" << std::endl
              << "   -q      --quiet                Do not print messages into console" << std::endl
              << "   -v      --verbose              Print detailed information" << std::endl
              << "           --no-color             Do not use decorations to make console output better" << std::endl
              << "           --no-progress          Do not show progress information" << std::endl
              << "           --width     pixels     Sets the viewport horizontal dimension (in pixels)" << std::endl
              << "           --height    pixels     Sets the viewport vertical dimension (in pixels)" << std::endl
              << "           --eye       x y z      Sets the position of the camera" << std::endl
              << "           --dir       x y z      Sets the direction vector of the camera" << std::endl
              << "           --up        x y z      Sets the up vector of the camera" << std::endl
              << "           --range     tmin tmax  Sets near and far clip range in world units" << std::endl
              << "           --camera    cam_type   Override camera type" << std::endl
              << "           --technique tech_type  Override technique/integrator type" << std::endl
              << "   -t      --target    target     Sets the target platform (default: autodetect CPU)" << std::endl
              << "   -d      --device    device     Sets the device to use on the selected platform (default: 0)" << std::endl
              << "           --cpu                  Use autodetected CPU target" << std::endl
              << "           --gpu                  Use autodetected GPU target" << std::endl
              << "           --debug                Same as --technique debug" << std::endl
              << "           --spp       spp        Enables benchmarking mode and sets the number of iterations based on the given spp" << std::endl
              << "           --spi       spi        Number of samples per iteration. This is only considered a hint for the underlying technique" << std::endl
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
              << "    path, debug, ao" << std::endl;
}

struct SectionTimer {
    Timer timer;
    size_t duration_ms = 0;

    inline void start() { timer.start(); }
    inline void stop() { duration_ms += timer.stopMS(); }
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
    std::optional<Vector3f> eye;
    std::optional<Vector3f> dir;
    std::optional<Vector3f> up;
    std::optional<Vector2f> trange;
    bool prettyConsole = true;
    bool quiet         = false;
    bool noProgress    = false;

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
            } else if (!strcmp(argv[i], "--no-progress")) {
                noProgress = true;
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

    IG_LOGGER.enableAnsiTerminal(prettyConsole);

    if (!quiet)
        std::cout << Build::getCopyrightString() << std::endl;

    if (in_file.empty()) {
        IG_LOG(L_ERROR) << "No input file given" << std::endl;
        return EXIT_FAILURE;
    }

    if (desired_spp <= 0) {
        IG_LOG(L_ERROR) << "No valid spp count given" << std::endl;
        return EXIT_FAILURE;
    }

    if (out_file.empty()) {
        IG_LOG(L_ERROR) << "No output file given" << std::endl;
        return EXIT_FAILURE;
    }

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
                  def.FOV, (float)def.FilmWidth / (float)def.FilmHeight,
                  clip(0), clip(1));
    runtime->setup();

    const size_t SPI          = runtime->samplesPerIteration();
    const size_t desired_iter = static_cast<size_t>(std::ceil(desired_spp / (float)SPI));

    if (desired_spp > 0 && (desired_spp % SPI) != 0)
        IG_LOG(L_WARNING) << "Given spp " << desired_spp << " is not a multiple of the spi " << SPI << ". Using spp " << desired_iter * SPI << " instead" << std::endl;

    StatusObserver observer(prettyConsole, 2, desired_iter * SPI /* Approx */);
    observer.begin();

    IG_LOG(L_INFO) << "Started rendering..." << std::endl;

    std::vector<double> samples_sec;

    SectionTimer timer_render;
    while (true) {
        if (!noProgress)
            observer.update(runtime->currentSampleCount());

        auto ticks = std::chrono::high_resolution_clock::now();

        timer_render.start();
        runtime->step(camera);
        timer_render.stop();

        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ticks).count();

        samples_sec.emplace_back(1000.0 * double(SPI * def.FilmWidth * def.FilmHeight) / double(elapsed_ms));
        if (samples_sec.size() == desired_iter)
            break;
    }

    if (!noProgress)
        observer.end();

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
            << stats->dump(runtime->currentIterationCount(), all_stats)
            << "  Iterations: " << runtime->currentIterationCount() << std::endl
            << "  SPP: " << runtime->currentSampleCount() << std::endl
            << "  SPI: " << SPI << std::endl
            << "  Time: " << timer_all.duration_ms << "ms" << std::endl
            << "    Loading> " << timer_loading.duration_ms << "ms" << std::endl
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
