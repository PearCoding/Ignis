#include "Logger.h"
#include "device/DeviceManager.h"
#include "device/ICompilerDevice.h"
#include "device/IDeviceInterface.h"
#include "shader/ScriptCompiler.h"

#include <fstream>

// Will be populated by api_collector
extern IG_LIB const char* ig_api[];
extern IG_LIB const char* ig_api_paths[];

using namespace IG;

// Simple tool compiling each artic code independantly to check includes
int main(int argc, char** argv)
{
    // --------------------------------
    // Get device
    if (!DeviceManager::instance().init(/*TODO Parameter*/)) {
        IG_LOG(L_ERROR) << "Could not initialize devices" << std::endl;
        return EXIT_FAILURE;
    }

    const IDeviceInterface* interface = DeviceManager::instance().getDevice(Target::pickCPU().architecture());
    if (interface == nullptr) {
        IG_LOG(L_ERROR) << "Could not get requested device" << std::endl;
        return EXIT_FAILURE;
    }

    // Get compiler interface
    std::shared_ptr<ICompilerDevice> compilerDevice = std::shared_ptr<ICompilerDevice>{ interface->createCompilerDevice() };
    if (compilerDevice == nullptr) {
        IG_LOG(L_ERROR) << "Could not get compiler interface from requested device" << std::endl;
        return EXIT_FAILURE;
    }

    ScriptCompiler compiler(compilerDevice, {});

    const auto compile = [&](const std::string& script, const std::string& path) {
        std::cout << "---------------------------- " << path << " ---------------------------" << std::endl;
        const std::string s = compiler.prepare(script, path);

        const bool ret = compilerDevice->compile(
            ICompilerDevice::Settings{
                .OptimizationLevel = 0,
                .Verbose           = IG_LOGGER.verbosity() == L_DEBUG },
            s);

        if (!ret)
            IG_LOG(L_ERROR) << "Failed to compile '" << path << "'" << std::endl;
        return ret;
    };

    // Prepare and compile
    if (argc <= 1) {
        for (int i = 0; ig_api_paths[i]; ++i) {
            if (!compile(ig_api[i], ig_api_paths[i]))
                break;
        }
    } else if (argc <= 2) {
        compiler.setStdLibPath(argv[1]);
        for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(argv[1])) {
            if (dirEntry.path().extension() == ".art") {
                std::stringstream stream;
                stream << std::fstream(dirEntry.path()).rdbuf();

                if (!compile(stream.str(), std::filesystem::relative(dirEntry.path(), argv[1]).generic_string()))
                    break;
            }
        }
    } else {
        compiler.setStdLibPath(argv[1]);
    
        std::stringstream stream;
        stream << std::fstream(argv[2]).rdbuf();

        compile(stream.str(), std::filesystem::relative(argv[2], argv[1]).generic_string());
    }

    // --------------------------------

    return EXIT_SUCCESS;
}
