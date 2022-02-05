#include "Logger.h"
#include "Runtime.h"
#include "config/Build.h"

#include <fstream>
#include <iterator>
#include <sstream>

using namespace IG;

static inline void check_arg(int argc, char** argv, int arg, int n)
{
    if (arg + n >= argc)
        IG_LOG(L_ERROR) << "Option '" << argv[arg] << "' expects " << n << " arguments, got " << (argc - arg) << std::endl;
}

static inline void version()
{
    std::cout << "igtrace " << Build::getVersionString() << std::endl;
}

static inline void usage()
{
    std::cout
        << "igtrace - Ignis Command Line Tracer" << std::endl
        << Build::getCopyrightString() << std::endl
        << "Usage: igtrace [options] file" << std::endl
        << "Available options:" << std::endl
        << "   -h      --help                   Shows this message" << std::endl
        << "           --version                Show version and exit" << std::endl
        << "   -q      --quiet                  Do not print messages into console" << std::endl
        << "   -v      --verbose                Print detailed information" << std::endl
        << "           --no-color               Do not use decorations to make console output better" << std::endl
        << "   -t      --target   target        Sets the target platform (default: autodetect CPU)" << std::endl
        << "   -d      --device   device        Sets the device to use on the selected platform (default: 0)" << std::endl
        << "           --cpu                    Use autodetected CPU target" << std::endl
        << "           --gpu                    Use autodetected GPU target" << std::endl
        << "   -n      --count    count         Samples per ray. Default is 1" << std::endl
        << "   -i      --input    list.txt      Read list of rays from file instead of the standard input" << std::endl
        << "   -o      --output   radiance.txt  Write radiance for each ray into file instead of standard output" << std::endl;
}

static inline float safe_rcp(float x)
{
    constexpr float min_rcp = 1e-8f;
    if ((x > 0 ? x : -x) < min_rcp) {
        return std::signbit(x) ? -std::numeric_limits<float>::max() : std::numeric_limits<float>::max();
    } else {
        return 1 / x;
    }
}

static std::vector<Ray> read_input(std::istream& is, bool file)
{
    std::vector<Ray> rays;
    while (true) {
        if (!file)
            std::cout << ">> ";

        std::string line;
        if (!std::getline(is, line) || line.empty())
            break;

        std::vector<float> data;
        std::stringstream stream(line);

        std::copy(std::istream_iterator<float>(stream),
                  std::istream_iterator<float>(),
                  std::back_inserter(data));

        if (data.size() < 6) {
            if (&is == &std::cin)
                std::cout << "Invalid input" << std::endl;
            continue; // Ignore
        }

        Ray ray;
        ray.Origin(0)    = data[0];
        ray.Origin(1)    = data[1];
        ray.Origin(2)    = data[2];
        ray.Direction(0) = data[3];
        ray.Direction(1) = data[4];
        ray.Direction(2) = data[5];

        ray.Range(0) = data.size() > 6 ? data[6] : 0;
        ray.Range(1) = data.size() > 7 ? data[7] : 0;

        if (ray.Range(1) <= ray.Range(0))
            ray.Range(1) = std::numeric_limits<float>::max();

        rays.push_back(ray);
    }

    return rays;
}

static void write_output(std::ostream& is, const float* data, size_t count, uint32 spp)
{
    for (size_t i = 0; i < count; ++i) {
        is << data[3 * i + 0] / spp << " " << data[3 * i + 1] / spp << " " << data[3 * i + 2] / spp << std::endl;
    }
}

int main(int argc, char** argv)
{
    if (argc <= 1) {
        usage();
        return EXIT_SUCCESS;
    }

    std::string scene_file;
    uint32 sample_count = 1;
    std::string ray_file;
    std::string out_file;
    RuntimeOptions opts;
    bool quiet = false;

    opts.OverrideCamera = "list";

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-n") || !strcmp(argv[i], "--count")) {
                check_arg(argc, argv, i, 1);
                sample_count = strtoul(argv[++i], nullptr, 10);
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                check_arg(argc, argv, i, 1);
                ++i;
                out_file = argv[i];
            } else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--input")) {
                check_arg(argc, argv, i, 1);
                ++i;
                ray_file = argv[i];
            } else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet")) {
                quiet = true;
                IG_LOGGER.setQuiet(true);
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                IG_LOGGER.setVerbosity(L_DEBUG);
            } else if (!strcmp(argv[i], "--no-color")) {
                IG_LOGGER.enableAnsiTerminal(false);
            } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                usage();
                return EXIT_SUCCESS;
            } else if (!strcmp(argv[i], "--version")) {
                version();
                return EXIT_SUCCESS;
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
                opts.Device = strtoul(argv[i], nullptr, 10);
            } else if (!strcmp(argv[i], "--cpu")) {
                opts.RecommendCPU = true;
                opts.RecommendGPU = false;
            } else if (!strcmp(argv[i], "--gpu")) {
                opts.RecommendCPU = false;
                opts.RecommendGPU = true;
            } else {
                IG_LOG(L_ERROR) << "Unknown option '" << argv[i] << "'" << std::endl;
                return EXIT_FAILURE;
            }
        } else {
            if (scene_file.empty()) {
                scene_file = argv[i];
            } else {
                IG_LOG(L_ERROR) << "Unexpected argument '" << argv[i] << "'" << std::endl;
                return EXIT_FAILURE;
            }
        }
    }

    // Fix samples per iteration to 1
    opts.SPI = 1;

    if (!quiet)
        std::cout << Build::getCopyrightString() << std::endl;

    std::vector<Ray> rays;
    if (ray_file.empty()) {
        rays = read_input(std::cin, false);
    } else {
        std::ifstream stream(ray_file);
        rays = read_input(stream, true);
    }

    if (rays.empty()) {
        IG_LOG(L_ERROR) << "No rays given" << std::endl;
        return EXIT_FAILURE;
    }

    opts.OverrideFilmSize = { (uint32)rays.size(), 1 };
    std::unique_ptr<Runtime> runtime;
    try {
        runtime = std::make_unique<Runtime>(scene_file, opts);
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    runtime->setup();

    const size_t SPP = runtime->samplesPerIteration();
    sample_count     = static_cast<size_t>(std::ceil(sample_count / (float)SPP));

    std::vector<float> accum_data;
    std::vector<float> iter_data;
    for (uint32 iter = 0; iter < sample_count; ++iter) {
        if (iter == 0)
            runtime->clearFramebuffer();
        runtime->trace(rays, iter_data);

        if (accum_data.size() != iter_data.size())
            accum_data.resize(iter_data.size(), 0.0f);
        for (size_t i = 0; i < iter_data.size(); ++i)
            accum_data[i] = iter_data[i];
    }

    // Extract data
    if (out_file.empty()) {
        write_output(std::cout, accum_data.data(), rays.size(), runtime->currentIterationCount());
    } else {
        std::ofstream stream(out_file);
        write_output(stream, accum_data.data(), rays.size(), runtime->currentIterationCount());
    }

    return EXIT_SUCCESS;
}
