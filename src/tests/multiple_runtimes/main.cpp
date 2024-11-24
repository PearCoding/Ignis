#include "Logger.h"
#include "ProgramOptions.h"
#include "Runtime.h"

using namespace IG;

// This application just tests if multiple runtimes can be used sequentially in a single run
// The test is passed if it runs without problems :P
// Every even number will be gpu, every odd will be cpu (if possible)
int main(int argc, char** argv)
{
    constexpr int SPP = 8;

    std::vector<Path> scenes;
    for (int k = 1; k < argc; ++k) {
        if (strcmp(argv[k], "--verbose") == 0)
            IG_LOGGER.setVerbosity(L_DEBUG);
        else
            scenes.push_back(argv[k]);
    }

    for (const Path& scene : scenes) {
        IG_LOG(L_INFO) << "Scene " << scene << std::endl;

        for (int i = 0; i < 2; ++i) {
            std::unique_ptr<Runtime> runtime;
            try {
                RuntimeOptions opts = RuntimeOptions::makeDefault();
                if (i % 2 == 1) {
                    opts.Target = Target::pickGPU();
                    if (!opts.Target.isValid() || opts.Target.gpuArchitecture() == GPUArchitecture::Unknown)
                        opts.Target = Target::pickBest();
                } else {
                    opts.Target = Target::pickCPU();
                }

                runtime = std::make_unique<Runtime>(opts);
            } catch (const std::exception& e) {
                IG_LOG(L_ERROR) << e.what() << std::endl;
                return EXIT_FAILURE;
            }

            if (!runtime->loadFromFile(scene)) {
                IG_LOG(L_ERROR) << "Failed loading" << std::endl;
                return EXIT_FAILURE;
            }

            while (runtime->currentSampleCount() < SPP)
                runtime->step();
        }
    }

    return EXIT_SUCCESS;
}
