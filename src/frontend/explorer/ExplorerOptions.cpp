#include "ExplorerOptions.h"
#include "EnumValidator.h"
#include "Runtime.h"
#include "config/Build.h"

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

ExplorerOptions::ExplorerOptions(int argc, char** argv, const std::string& desc)
{
    CLI::App app{ desc, argc >= 1 ? argv[0] : "unknown" };
    argv = app.ensure_utf8(argv);

    app.positionals_at_end(false);
    app.set_version_flag("--version", Build::getBuildString());
    app.set_help_flag("-h,--help", "Shows help message and exit");

    app.add_option("file", InputFile, "Scene file to initially load. Can be a Ignis scene file or a glTF file.")->check(CLI::ExistingFile);
    app.add_flag("-q,--quiet", Quiet, "Do not print messages into console");
    app.add_flag_callback(
        "-v,--verbose", [&]() { VerbosityLevel = L_DEBUG; }, "Set the verbosity level to 'debug'. Shortcut for --log-level debug");

    app.add_option("--log-level", VerbosityLevel, "Set the verbosity level")->transform(EnumValidator(LogLevelMap, CLI::ignore_case));

    app.add_flag("--no-color", NoColor, "Do not use decorations to make console output better");

    app.add_option("--width", WindowWidth, "Set initial window width");
    app.add_option("--height", WindowHeight, "Set initial window height");
    app.add_option("--dpi", DPI, "Optional scaling factor for the UI. If not set, it will be acquired automatically");

    app.add_flag("--stats", AcquireStats, "Acquire useful stats alongside rendering. Will be dumped at the end of the rendering session");
    app.add_flag("--stats-full", AcquireFullStats, "Acquire all stats alongside rendering. Will be dumped at the end of the rendering session");
    app.add_flag("--debug-trace", DebugTrace, "Dump information regarding calls on the device. Will slow down execution and produce a lot of output!");

    app.add_flag("--dump-shader", DumpShader, "Dump produced shaders to files in the current working directory");
    app.add_flag("--dump-shader-full", DumpFullShader, "Dump produced shaders with standard library to files in the current working directory");
    app.add_flag("--dump-registry", DumpRegistry, "Dump global registry to standard output");
    app.add_flag("--dump-registry-full", DumpFullRegistry, "Dump global and internal constructed registry to standard output");

    app.add_flag("--no-cache", NoCache, "Disable filesystem cache usage, which saves large computations for future runs and loads data from previous runs");
    app.add_option("--cache-dir", CacheDir, "Set directory to cache large computations explicitly, else a directory based on the input file will be used");

    app.add_option("-O,--shader-optimization", ShaderOptimizationLevel, "Level of optimization applied to shaders. Range is [0, 3]. Level 0 will also add debug information")->default_val(ShaderOptimizationLevel);
    app.add_option("--shader-threads", ShaderCompileThreads, "Number of threads to use to compile large shaders. Set to 0 to detect automatically")->default_val(ShaderCompileThreads);

    app.add_option("--specialization", Specialization, "Set the type of specialization. Force will increase compile time drastically for potential runtime optimization.")->transform(EnumValidator(SpecializationModeMap, CLI::ignore_case))->default_str("default");
    app.add_flag_callback(
        "--force-specialization", [&]() { this->Specialization = RuntimeOptions::SpecializationMode::Force; },
        "Enforce specialization for parameters in shading tree. This will increase compile time drastically for potential runtime optimization");
    app.add_flag_callback(
        "--disable-specialization", [&]() { this->Specialization = RuntimeOptions::SpecializationMode::Disable; },
        "Disables specialization for parameters in shading tree. This might decrease compile time drastically for worse runtime optimization");

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

    IG_LOGGER.setQuiet(Quiet);
    IG_LOGGER.setVerbosity(VerbosityLevel);
    IG_LOGGER.enableAnsiTerminal(!NoColor);
}

void ExplorerOptions::populate(RuntimeOptions& options) const
{
    options.AcquireStats     = AcquireStats || AcquireFullStats;
    options.DebugTrace       = DebugTrace;
    options.DumpShader       = DumpShader || DumpFullShader;
    options.DumpShaderFull   = DumpFullShader;
    options.DumpRegistry     = DumpRegistry || DumpFullRegistry;
    options.DumpRegistryFull = DumpFullRegistry;

    options.Specialization = Specialization;

    options.EnableCache = !NoCache;
    options.CacheDir    = CacheDir;

    options.ShaderOptimizationLevel = std::min<size_t>(3, ShaderOptimizationLevel);
    options.ShaderCompileThreads    = ShaderCompileThreads;
}
} // namespace IG