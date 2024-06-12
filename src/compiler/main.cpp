#include <fstream>
#include <iostream>

#include "CompilerOptions.h"
#include "Logger.h"
#include "device/DeviceManager.h"
#include "device/ICompilerDevice.h"
#include "device/IDeviceInterface.h"

using namespace IG;

static std::string getInput(std::istream& inputStream)
{
    std::stringstream stream;
    for (std::string line; std::getline(inputStream, line);) {
        stream << line << std::endl;
    }
    return stream.str();
}

int main(int argc, char** argv)
{
    CompilerOptions cmd(argc, argv, "Ignis JIT Compiler");
    if (cmd.ShouldExit)
        return EXIT_SUCCESS;

    if (!cmd.Quiet && !cmd.NoLogo)
        std::cout << Build::getCopyrightString() << std::endl;

    // --------------------------------
    std::string input;
    if (cmd.InputFile.empty()) {
        // Read from std::cin
        input = getInput(std::cin);
    } else {
        std::ifstream istream(cmd.InputFile);
        input = getInput(istream);
    }

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

    // --------------------------------
    const bool ret = compilerDevice->compile(
        ICompilerDevice::Settings{
            .OptimizationLevel = (int)cmd.OptimizationLevel,
            .Verbose           = IG_LOGGER.verbosity() == L_DEBUG },
        input);

    if (!ret) {
        IG_LOG(L_ERROR) << "Failed to compile input" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}