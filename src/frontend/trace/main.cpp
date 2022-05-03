#include "Logger.h"
#include "ProgramOptions.h"
#include "Runtime.h"
#include "config/Build.h"

#include <fstream>
#include <iterator>
#include <sstream>

#ifndef IG_OS_WINDOWS
#include <unistd.h>
#endif

using namespace IG;

static std::vector<Ray> read_input(std::istream& is, bool print_prefix)
{
    std::vector<Ray> rays;
    while (true) {
        if (print_prefix)
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

        if (is.eof())
            break;
    }

    return rays;
}

static void write_output(std::ostream& is, const float* data, size_t count, size_t spp)
{
    for (size_t i = 0; i < count; ++i)
        is << std::scientific << data[3 * i + 0] / spp << "\t" << data[3 * i + 1] / spp << "\t" << data[3 * i + 2] / spp << std::endl;
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

#ifndef IG_OS_WINDOWS
    const bool isAtty = isatty(fileno(stdin));
#else
    const bool isAtty = true; // Just assume it
#endif
    const bool isInteractive = cmd.InputRay.empty();
    bool firstRound          = true;

    while (true) {
        std::vector<Ray> rays;
        if (isInteractive) {
            rays = read_input(std::cin, isAtty);
        } else {
            std::ifstream stream(cmd.InputRay);
            rays = read_input(stream, false);
        }

        if (rays.empty()) {
            if (firstRound || !isInteractive)
                IG_LOG(L_ERROR) << "No rays given" << std::endl;
            return EXIT_FAILURE;
        }

        opts.OverrideFilmSize = { (uint32)rays.size(), 1 };
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

        const size_t SPI          = runtime->samplesPerIteration();
        const size_t desired_iter = std::max<size_t>(1, static_cast<size_t>(std::ceil(cmd.SPP.value_or(1) / (float)SPI)));

        if (cmd.SPP.has_value() && (cmd.SPP.value() % SPI) != 0)
            IG_LOG(L_WARNING) << "Given spp " << cmd.SPP.value() << " is not a multiple of the spi " << SPI << ". Using spp " << desired_iter * SPI << " instead" << std::endl;

        std::vector<float> accum_data;
        std::vector<float> iter_data;
        for (size_t iter = 0; iter < desired_iter; ++iter) {
            runtime->trace(rays, iter_data);

            if (accum_data.size() != iter_data.size())
                accum_data.resize(iter_data.size(), 0.0f);
            for (size_t i = 0; i < iter_data.size(); ++i)
                accum_data[i] = iter_data[i];
        }

        if (accum_data.size() != rays.size() * 3) {
            IG_LOG(L_FATAL) << "Got trace output size " << accum_data.size() << " but expected " << rays.size() * 3 << std::endl;
            return EXIT_FAILURE;
        }

        // Extract data
        if (cmd.Output.empty()) {
            write_output(std::cout, accum_data.data(), rays.size(), runtime->currentIterationCount());
        } else {
            std::ofstream stream(cmd.Output, firstRound ? std::ofstream::out : (std::ofstream::out | std::ofstream::app));
            write_output(stream, accum_data.data(), rays.size(), runtime->currentIterationCount());
        }

        firstRound = false;
        if (!isInteractive || !isAtty)
            break;
    }

    return EXIT_SUCCESS;
}
