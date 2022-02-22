#include "Logger.h"
#include "ProgramOptions.h"
#include "Runtime.h"
#include "config/Build.h"

#include <fstream>
#include <iterator>
#include <sstream>

using namespace IG;

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
    ProgramOptions cmd(argc, argv, ApplicationType::Trace, "Command Line Tracer");
    if (cmd.ShouldExit)
        return EXIT_SUCCESS;

    RuntimeOptions opts;
    cmd.populate(opts);

    // Fix samples per iteration to 1
    opts.SPI      = 1;
    opts.IsTracer = true;

    if (!cmd.Quiet)
        std::cout << Build::getCopyrightString() << std::endl;

    std::vector<Ray> rays;
    if (cmd.InputRay.empty()) {
        rays = read_input(std::cin, false);
    } else {
        std::ifstream stream(cmd.InputRay);
        rays = read_input(stream, true);
    }

    if (rays.empty()) {
        IG_LOG(L_ERROR) << "No rays given" << std::endl;
        return EXIT_FAILURE;
    }

    opts.OverrideFilmSize = { (uint32)rays.size(), 1 };
    std::unique_ptr<Runtime> runtime;
    try {
        runtime = std::make_unique<Runtime>(cmd.InputScene, opts);
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    runtime->setup();

    const size_t SPI    = runtime->samplesPerIteration();
    size_t sample_count = static_cast<size_t>(std::ceil(cmd.SPP.value_or(0) / (float)SPI));

    std::vector<float> accum_data;
    std::vector<float> iter_data;
    for (uint32 iter = 0; iter < sample_count; ++iter) {
        if (iter == 0)
            runtime->reset();
        runtime->trace(rays, iter_data);

        if (accum_data.size() != iter_data.size())
            accum_data.resize(iter_data.size(), 0.0f);
        for (size_t i = 0; i < iter_data.size(); ++i)
            accum_data[i] = iter_data[i];
    }

    // Extract data
    if (cmd.Output.empty()) {
        write_output(std::cout, accum_data.data(), rays.size(), runtime->currentIterationCount());
    } else {
        std::ofstream stream(cmd.Output);
        write_output(stream, accum_data.data(), rays.size(), runtime->currentIterationCount());
    }

    return EXIT_SUCCESS;
}
