#include "GlareOptions.h"
#include "EnumValidator.h"
#include "Runtime.h"
#include "config/Build.h"
#include "loader/Transpiler.h"

namespace IG {
static const std::map<std::string, LogLevel> LogLevelMap{ { "fatal", L_FATAL }, { "error", L_ERROR }, { "warning", L_WARNING }, { "info", L_INFO }, { "debug", L_DEBUG } };
static const std::map<std::string, RuntimeOptions::SpecializationMode> SpecializationModeMap{ { "default", RuntimeOptions::SpecializationMode::Default }, { "force", RuntimeOptions::SpecializationMode::Force }, { "disable", RuntimeOptions::SpecializationMode::Disable } };

static void handleListCLIOptions(const CLI::App& app)
{
    const auto options = app.get_options();
    for (auto option : options) {
        const auto shortNames = option->get_snames();
        for (const auto& s : shortNames)
            std::cout << "-" << s << std::endl;

        const auto longNames = option->get_lnames();
        for (const auto& s : longNames)
            std::cout << "--" << s << std::endl;
    }
}

GlareOptions::GlareOptions(int argc, char** argv)
{
    bool useCPU        = false;
    bool useGPU        = false;
    uint32 threadCount = 0;
    uint32 vectorWidth = 0;
    uint32 device      = 0;
    std::string gpu_arch;
    std::string cpu_arch;

    CLI::App app{ "Tool to estimate glare risk related metrics", argc >= 1 ? argv[0] : "unknown" };
    argv = app.ensure_utf8(argv);

    app.positionals_at_end(false);
    app.set_version_flag("--version", Build::getBuildString());
    app.set_help_flag("-h,--help", "Shows help message and exit");

    app.add_option("image", Input, "Imge file to load. Can be a .exr, .hdr or other supported image file.")->required()->check(CLI::ExistingFile);
    app.add_flag("-q,--quiet", Quiet, "Do not print messages into console");
    app.add_flag_callback(
        "-v,--verbose", [&]() { VerbosityLevel = L_DEBUG; }, "Set the verbosity level to 'debug'. Shortcut for --log-level debug");

    app.add_option("--log-level", VerbosityLevel, "Set the verbosity level")->transform(EnumValidator(LogLevelMap, CLI::ignore_case));
    app.add_flag("--no-unused", NoUnused, "Do not warn about unused properties");

    app.add_flag("--no-color", NoColor, "Do not use decorations to make console output better");

    app.add_option("-b,--multiplier", Multiplier, "Glare source multiplier used to detect glare sources")->default_val(Multiplier);
    app.add_option("--vertical-illuminance", VerticalIlluminance, "Override the automatically computed vertical illuminance used for metric computations");

    app.add_flag("--cpu", useCPU, "Use CPU as target only");
    app.add_flag("--gpu", useGPU, "Use GPU as target only");
    app.add_option("--gpu-arch", gpu_arch, "Explicitly set GPU architecture to use. Will not check if available or not")->check(CLI::IsMember(IG::Target::getAvailableGPUArchitectureNames(), CLI::ignore_case));
    app.add_option("--gpu-device", device, "Pick GPU device to use on the selected platform")->default_val(0);
    app.add_option("--cpu-arch", cpu_arch, "Explicitly set CPU architecture to use. Only choose a host compatible architecture, else the application will crash")->check(CLI::IsMember(IG::Target::getAvailableCPUArchitectureNames(), CLI::ignore_case));
    app.add_option("--cpu-threads", threadCount, "Number of threads used on a CPU target. Set to 0 to detect automatically")->default_val(threadCount);
    app.add_option("--cpu-vectorwidth", vectorWidth, "Number of vector lanes used on a CPU target. Set to 0 to detect automatically")->default_val(vectorWidth);

    app.add_flag("--no-cache", NoCache, "Disable filesystem cache usage, which saves large computations for future runs and loads data from previous runs");
    app.add_option("--cache-dir", CacheDir, "Set directory to cache large computations explicitly, else a directory based on the input file will be used");

    app.add_option("--script-dir", ScriptDir, "Override internal script standard library by '.art' files from the given directory");

    app.add_option("-O,--shader-optimization", ShaderOptimizationLevel, "Level of optimization applied to shaders. Range is [0, 3]. Level 0 will also add debug information")->default_val(ShaderOptimizationLevel);
    app.add_option("--shader-threads", ShaderCompileThreads, "Number of threads to use to compile large shaders. Set to 0 to detect automatically")->default_val(ShaderCompileThreads);

    // Add some hidden commandline parameters
    bool listCLI = false;
    auto grp     = app.add_option_group("");
    grp->add_flag("--list-cli-options", listCLI);

    auto handleHiddenExtras = [&]() {
        if (listCLI) {
            handleListCLIOptions(app);
            ShouldExit = true;
        }
    };

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        handleHiddenExtras();
        if (!ShouldExit) // If true, exit is already handled
            app.exit(e);
        ShouldExit = true;
        return;
    }

    // Handle hidden options
    handleHiddenExtras();
    if (ShouldExit)
        return;

    // Handle explicit target choices
    const CPUArchitecture fix_cpu_arch = IG::Target::getCPUArchitectureFromString(cpu_arch);
    const GPUArchitecture fix_gpu_arch = IG::Target::getGPUArchitectureFromString(gpu_arch);

    if (fix_cpu_arch != CPUArchitecture::Unknown)
        useCPU = true;
    if (fix_gpu_arch != GPUArchitecture::Unknown)
        useGPU = true;

    // Setup target
    if (useGPU) {
        if (fix_gpu_arch != GPUArchitecture::Unknown)
            Target = IG::Target::makeGPU(fix_gpu_arch, 0 /* Will be set later */);
        else
            Target = IG::Target::pickGPU(device);

    } else if (useCPU) {
        if (fix_cpu_arch != CPUArchitecture::Unknown)
            Target = IG::Target::makeCPU(fix_cpu_arch, 0 /* Will be set later */, 1 /* Will be set later*/);
        else
            Target = IG::Target::pickCPU();
    } else {
        Target = IG::Target::pickBest();
    }

    Target.setDevice((size_t)device);
    Target.setThreadCount((size_t)threadCount);

    if (vectorWidth >= 1)
        Target.setVectorWidth((size_t)vectorWidth);
}

void GlareOptions::populate(RuntimeOptions& options) const
{
    IG_LOGGER.setQuiet(Quiet);
    IG_LOGGER.setVerbosity(VerbosityLevel);
    IG_LOGGER.enableAnsiTerminal(!NoColor);

    options.IsTracer      = false;
    options.IsInteractive = false;

    options.Target           = Target;
    options.AcquireStats     = false;
    options.DebugTrace       = false;
    options.DumpShader       = false;
    options.DumpShaderFull   = false;
    options.DumpRegistry     = false;
    options.DumpRegistryFull = false;
    options.SPI              = 1;

    options.DisableStandardAOVs  = true;
    options.Denoiser.Enabled     = false;
    options.Denoiser.HighQuality = !options.IsInteractive;

    options.EnableCache = !NoCache;
    options.CacheDir    = CacheDir;

    options.ScriptDir               = ScriptDir;
    options.ShaderOptimizationLevel = std::min<size_t>(3, ShaderOptimizationLevel);
    options.ShaderCompileThreads    = ShaderCompileThreads;

    options.WarnUnused = !NoUnused;

    // Check for power of two and round up if not the case
    uint64_t vectorWidth = options.Target.vectorWidth();
    if (vectorWidth == 2) {
        vectorWidth = 4;
        IG_LOG(L_WARNING) << "Given vector width is 2, which is invalid. Setting it to " << vectorWidth << std::endl;
    } else if ((vectorWidth & (vectorWidth - 1)) != 0) {
        vectorWidth--;
        vectorWidth |= vectorWidth >> 1;
        vectorWidth |= vectorWidth >> 2;
        vectorWidth |= vectorWidth >> 4;
        vectorWidth |= vectorWidth >> 8;
        vectorWidth |= vectorWidth >> 16;
        vectorWidth |= vectorWidth >> 32;
        vectorWidth++;

        IG_LOG(L_WARNING) << "Given vector width is not power of 2. Setting it to " << vectorWidth << std::endl;
    }
    options.Target.setVectorWidth((size_t)vectorWidth);
}

} // namespace IG