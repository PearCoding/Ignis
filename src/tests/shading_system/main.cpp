#include "device/DeviceManager.h"
#include "device/IDeviceInterface.h"
#include "loader/Transpiler.h"
#include "shader/ScriptCompiler.h"

#include "Logger.h"

using namespace IG;
int main(int argc, const char** argv)
{
    IG_LOGGER.setVerbosity(LogLevel::L_DEBUG);

    const std::string shader = Transpiler::generateTestShader();
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--dump") == 0)
            std::cout << shader << std::endl;
    }

    if (!DeviceManager::instance().init())
        return EXIT_FAILURE;

    const auto target          = Target::pickCPU();
    const auto deviceInterface = DeviceManager::instance().getDevice(target.architecture());
    if (deviceInterface == nullptr) {
        std::cerr << "Could not get target " << target.toString() << std::endl;
        return EXIT_FAILURE;
    }

    const auto compilerDevice = std::shared_ptr<ICompilerDevice>{ deviceInterface->createCompilerDevice() };

    ScriptCompiler compiler(compilerDevice);
    compiler.setVerbose(true);
    compiler.setOptimizationLevel(0);

    const std::string prepared_shader = compiler.prepare(shader);
    void* func                        = compiler.compile(prepared_shader, "_var_test_");

    return func != nullptr ? EXIT_SUCCESS : EXIT_FAILURE;
}