#include "Logger.h"
#include "device/DeviceManager.h"
#include "device/ICompilerDevice.h"
#include "device/IDeviceInterface.h"
#include "shader/ScriptCompiler.h"

using namespace IG;

extern "C" int error_count;
extern const char* ig_test_shader[];

int main(int argc, char** argv)
{
    IG_LOGGER.setVerbosity(L_DEBUG); // This is a test, so go full verbose

    Target target = Target::pickBest();
    bool run      = true;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-gpu") == 0)
            target = Target::pickCPU();
        else if (std::strcmp(argv[i], "--no-run") == 0)
            run = false;
    }

    if (!DeviceManager::instance().init(/*TODO Parameter*/)) {
        IG_LOG(L_ERROR) << "Could not initialize devices" << std::endl;
        return EXIT_FAILURE;
    }

    const IDeviceInterface* interface = DeviceManager::instance().getDevice(target.architecture());
    if (interface == nullptr) {
        IG_LOG(L_ERROR) << "Could not get requested device" << std::endl;
        return EXIT_FAILURE;
    }

    // Get compiler interface
    const std::shared_ptr<ICompilerDevice> compilerDevice = std::shared_ptr<ICompilerDevice>{ interface->createCompilerDevice() };
    if (compilerDevice == nullptr) {
        IG_LOG(L_ERROR) << "Could not get compiler interface from requested device" << std::endl;
        return EXIT_FAILURE;
    }

    // Setup shader
    std::stringstream shaderStream;
    for (int i = 0; ig_test_shader[i]; ++i)
        shaderStream << ig_test_shader[i] << std::endl;

    shaderStream << "static NoGPU : bool = " << (target.isGPU() ? "false" : "true") << ";" << std::endl;

    ScriptCompiler compiler(compilerDevice);
    const std::string source = compiler.prepare(shaderStream.str());

    void* test_main = compiler.compile(source, "test_main");
    if (!test_main)
        return EXIT_FAILURE;

    if (!run)
        return EXIT_SUCCESS;

    // Get render interface
    const std::shared_ptr<IRenderDevice> renderDevice = std::shared_ptr<IRenderDevice>{ interface->createRenderDevice(IRenderDevice::SetupSettings{
        .Target        = target,
        .AcquireStats  = false,
        .DebugTrace    = false,
        .IsInteractive = false }) };

    if (renderDevice == nullptr) {
        IG_LOG(L_ERROR) << "Could not get render interface from requested device" << std::endl;
        return EXIT_FAILURE;
    }

    renderDevice->runPass(ShaderOutput<void*>{
                              .Exec          = test_main,
                              .LocalRegistry = nullptr },
                          nullptr);

    if (error_count != 0)
        std::cout << error_count << " failed tests!" << std::endl;
    else
        std::cout << "All tests passed :)" << std::endl;

    return error_count;
}
