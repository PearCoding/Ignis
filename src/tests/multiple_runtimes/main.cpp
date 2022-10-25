#include "Logger.h"
#include "ProgramOptions.h"
#include "Runtime.h"

using namespace IG;

// This application just tests if multiple runtimes can be used sequentially in a single run
// The test is passed if it runs without problems :P
int main(int argc, char** argv)
{
    constexpr int SPP = 8;
    for (int k = 1; k < argc; ++k) {
        IG_LOG(L_INFO) << "Scene " << k << std::endl;

        std::unique_ptr<Runtime> runtime;
        try {
            RuntimeOptions opts = RuntimeOptions::makeDefault();
            runtime = std::make_unique<Runtime>(opts);
        } catch (const std::exception& e) {
            IG_LOG(L_ERROR) << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        if (!runtime->loadFromFile(argv[k])) {
            IG_LOG(L_ERROR) << "Failed loading" << std::endl;
            return EXIT_FAILURE;
        }

        const size_t SPI    = runtime->samplesPerIteration();
        size_t sample_count = static_cast<size_t>(std::ceil(SPP / (float)SPI));

        for (uint32 iter = 0; iter < sample_count; ++iter)
            runtime->step();
    }

    return EXIT_SUCCESS;
}
