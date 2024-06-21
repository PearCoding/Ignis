#include "ProgramOptions.h"
#include "EnumValidator.h"
#include "Runtime.h"
#include "config/Build.h"
#include "loader/Transpiler.h"

namespace IG {
static const std::map<std::string, LogLevel> LogLevelMap{ { "fatal", L_FATAL }, { "error", L_ERROR }, { "warning", L_WARNING }, { "info", L_INFO }, { "debug", L_DEBUG } };
static const std::map<std::string, SPPMode> SPPModeMap{ { "fixed", SPPMode::Fixed }, { "capped", SPPMode::Capped }, { "continuous", SPPMode::Continuous } };
static const std::map<std::string, RuntimeOptions::SpecializationMode> SpecializationModeMap{ { "default", RuntimeOptions::SpecializationMode::Default }, { "force", RuntimeOptions::SpecializationMode::Force }, { "disable", RuntimeOptions::SpecializationMode::Disable } };

static void handleListPExprVariables()
{
    std::cout << Transpiler::availableVariables() << std::endl;
}

static void handleListPExprFunctions()
{
    std::cout << Transpiler::availableFunctions() << std::endl;
}

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

ProgramOptions::ProgramOptions(int argc, char** argv, ApplicationType type, const std::string& desc)
{
    bool useCPU        = false;
    bool useGPU        = false;
    uint32 threadCount = 0;
    uint32 vectorWidth = 0;
    uint32 device      = 0;
    std::string gpu_arch;
    std::string cpu_arch;

    Type = type;

    CLI::App app{ desc, argc >= 1 ? argv[0] : "unknown" };

    app.positionals_at_end(false);
    app.set_version_flag("--version", Build::getBuildString());
    app.set_help_flag("-h,--help", "Shows help message and exit");

    app.add_option("scene", InputScene, "Scene file to load. Can be a Ignis scene file or a glTF file.")->required()->check(CLI::ExistingFile);
    app.add_flag("-q,--quiet", Quiet, "Do not print messages into console");
    app.add_flag_callback(
        "-v,--verbose", [&]() { VerbosityLevel = L_DEBUG; }, "Set the verbosity level to 'debug'. Shortcut for --log-level debug");

    app.add_option("--log-level", VerbosityLevel, "Set the verbosity level")->transform(EnumValidator(LogLevelMap, CLI::ignore_case));
    app.add_flag("--no-unused", NoUnused, "Do not warn about unused properties");

    app.add_flag("--no-color", NoColor, "Do not use decorations to make console output better");

    if (type == ApplicationType::CLI)
        app.add_flag("--no-progress", NoProgress, "Do not show progress information");

    if (type != ApplicationType::Trace) {
        const auto width  = app.add_option("--width", Width, "Set the viewport horizontal dimension (in pixels)");
        const auto height = app.add_option("--height", Height, "Set the viewport vertical dimension (in pixels)");

        width->needs(height);
        height->needs(width);

        app.add_option("--eye", Eye, "Set the position of the camera");
        app.add_option("--dir", Dir, "Set the direction vector of the camera");
        app.add_option("--up", Up, "Set the up vector of the camera");

        app.add_option("--camera", CameraType, "Override camera type")->check(CLI::IsMember(Runtime::getAvailableCameraTypes(), CLI::ignore_case));
    }

    app.add_option("--technique", TechniqueType, "Override technique type")->check(CLI::IsMember(Runtime::getAvailableTechniqueTypes(), CLI::ignore_case));
    app.add_flag_callback(
        "--debug", [&]() { TechniqueType = "debug"; }, "Same as --technique debug");

    app.add_flag("--cpu", useCPU, "Use CPU as target only");
    app.add_flag("--gpu", useGPU, "Use GPU as target only");
    app.add_option("--gpu-arch", gpu_arch, "Explicitly set GPU architecture to use. Will not check if available or not")->check(CLI::IsMember(IG::Target::getAvailableGPUArchitectureNames(), CLI::ignore_case));
    app.add_option("--gpu-device", device, "Pick GPU device to use on the selected platform")->default_val(0);
    app.add_option("--cpu-arch", cpu_arch, "Explicitly set CPU architecture to use. Only choose a host compatible architecture, else the application will crash")->check(CLI::IsMember(IG::Target::getAvailableCPUArchitectureNames(), CLI::ignore_case));
    app.add_option("--cpu-threads", threadCount, "Number of threads used on a CPU target. Set to 0 to detect automatically")->default_val(threadCount);
    app.add_option("--cpu-vectorwidth", vectorWidth, "Number of vector lanes used on a CPU target. Set to 0 to detect automatically")->default_val(vectorWidth);

    app.add_option("--spp", SPP, "Number of samples per pixel a frame contains");
    app.add_option("--spi", SPI, "Number of samples per iteration. This is only considered a hint for the underlying technique");
    if (type == ApplicationType::View) {
        app.add_option("--spp-mode", SPPMode, "Sets the current spp mode")->transform(EnumValidator(SPPModeMap, CLI::ignore_case))->default_str("fixed");
        app.add_flag_callback(
            "--realtime", [&]() { this->SPPMode = SPPMode::Continuous; SPI = 1; SPP = 1; },
            "Same as setting SPPMode='Continuous', SPI=1 and SPP=1 to emulate realtime rendering");
    }
    if (type == ApplicationType::CLI)
        app.add_option("--time", RenderTime, "Instead of spp, specify the maximum time in seconds to render")->excludes("--spp");

    app.add_option("--seed", Seed, "Seed for the random generators. Depending on the technique this will enforce reproducibility");

    app.add_flag("--stats", AcquireStats, "Acquire useful stats alongside rendering. Will be dumped at the end of the rendering session");
    app.add_flag("--stats-full", AcquireFullStats, "Acquire all stats alongside rendering. Will be dumped at the end of the rendering session");

    app.add_flag("--debug-trace", DebugTrace, "Dump information regarding calls on the device. Will slow down execution and produce a lot of output!");

    app.add_flag("--dump-shader", DumpShader, "Dump produced shaders to files in the current working directory");
    app.add_flag("--dump-shader-full", DumpFullShader, "Dump produced shaders with standard library to files in the current working directory");
    app.add_flag("--dump-registry", DumpRegistry, "Dump global registry to standard output");
    app.add_flag("--dump-registry-full", DumpFullRegistry, "Dump global and internal constructed registry to standard output");

    app.add_flag("--no-cache", NoCache, "Disable filesystem cache usage, which saves large computations for future runs and loads data from previous runs");
    app.add_option("--cache-dir", CacheDir, "Set directory to cache large computations explicitly, else a directory based on the input file will be used");

    app.add_option("--script-dir", ScriptDir, "Override internal script standard library by '.art' files from the given directory");

    app.add_option("-O,--shader-optimization", ShaderOptimizationLevel, "Level of optimization applied to shaders. Range is [0, 3]. Level 0 will also add debug information")->default_val(ShaderOptimizationLevel);
    app.add_option("--shader-threads", ShaderCompileThreads, "Number of threads to use to compile large shaders. Set to 0 to detect automatically")->default_val(ShaderCompileThreads);

    app.add_flag("--add-env-light", AddExtraEnvLight, "Add additional constant environment light. This is automatically done for glTF scenes without any lights");
    app.add_option("--specialization", Specialization, "Set the type of specialization. Force will increase compile time drastically for potential runtime optimization.")->transform(EnumValidator(SpecializationModeMap, CLI::ignore_case))->default_str("default");
    app.add_flag_callback(
        "--force-specialization", [&]() { this->Specialization = RuntimeOptions::SpecializationMode::Force; },
        "Enforce specialization for parameters in shading tree. This will increase compile time drastically for potential runtime optimization");
    app.add_flag_callback(
        "--disable-specialization", [&]() { this->Specialization = RuntimeOptions::SpecializationMode::Disable; },
        "Disables specialization for parameters in shading tree. This might decrease compile time drastically for worse runtime optimization");

    if (type != ApplicationType::Trace) {
        app.add_flag("--denoise", Denoise, "Apply denoiser if available");
        app.add_flag("--glare", Glare, "Enable glare overlay");
    }

    if (type == ApplicationType::Trace) {
        app.add_option("-i,--input", InputRay, "Read list of rays from file instead of the standard input");
        app.add_option("-o,--output", Output, "Write radiance for each ray into file instead of standard output");
    } else {
        app.add_option("-o,--output", Output, "Writes the output image to a file");
    }

    if (type == ApplicationType::View)
        app.add_option("--dpi", DPI, "Optional scaling factor for the UI. If not set, it will be acquired automatically");

    // Add user entries
    app.add_option(
           "--Pi,--parameter-int", [&](const CLI::results_t& r) -> bool {
               if (r.size() != 2)
                   return false;
               const std::string name  = r.at(0);
               const std::string val_s = r.at(1);

               try {
                   const int value                 = std::stoi(val_s);
                   UserEntries.IntParameters[name] = value;
                   return true;
               } catch (...) {
                   return false;
               }
           },
           "Set integer value in global registry")
        ->expected(2);

    app.add_option(
           "--Pn,--parameter-num", [&](const CLI::results_t& r) -> bool {
               if (r.size() != 2)
                   return false;
               const std::string name  = r.at(0);
               const std::string val_s = r.at(1);

               try {
                   const float value                 = std::stof(val_s);
                   UserEntries.FloatParameters[name] = value;
                   return true;
               } catch (...) {
                   return false;
               }
           },
           "Set number value in global registry")
        ->expected(2);

    app.add_option(
           "--Pv,--parameter-vec", [&](const CLI::results_t& r) -> bool {
               if (r.size() != 4)
                   return false;
               const std::string name    = r.at(0);
               const std::string val_X_s = r.at(1);
               const std::string val_Y_s = r.at(2);
               const std::string val_Z_s = r.at(3);

               try {
                   const float x = std::stof(val_X_s);
                   const float y = std::stof(val_Y_s);
                   const float z = std::stof(val_Z_s);

                   UserEntries.VectorParameters[name] = Vector3f(x, y, z);
                   return true;
               } catch (...) {
                   return false;
               }
           },
           "Set vector value in global registry")
        ->expected(4);

    app.add_option(
           "--Pc,--parameter-col", [&](const CLI::results_t& r) -> bool {
               if (r.size() != 4 && r.size() != 5)
                   return false;
               const std::string name    = r.at(0);
               const std::string val_R_s = r.at(1);
               const std::string val_G_s = r.at(2);
               const std::string val_B_s = r.at(3);
               const std::string val_A_s = r.size() > 4 ? r.at(4) : "1";

               try {
                   const float col_r = std::stof(val_R_s);
                   const float col_g = std::stof(val_G_s);
                   const float col_b = std::stof(val_B_s);
                   const float col_a = std::stof(val_A_s);

                   UserEntries.ColorParameters[name] = Vector4f(col_r, col_g, col_b, col_a);
                   return true;
               } catch (...) {
                   return false;
               }
           },
           "Set color value in global registry")
        ->expected(4, 5);

    // Add some hidden commandline parameters
    bool listPExprVariables = false;
    bool listPExprFunctions = false;
    bool listCLI            = false;
    auto grp                = app.add_option_group("");
    grp->add_flag("--list-pexpr-variables", listPExprVariables);
    grp->add_flag("--list-pexpr-functions", listPExprFunctions);
    grp->add_flag("--list-cli-options", listCLI);

    auto handleHiddenExtras = [&]() {
        if (listPExprVariables) {
            handleListPExprVariables();
            ShouldExit = true;
        }
        if (listPExprFunctions) {
            handleListPExprFunctions();
            ShouldExit = true;
        }
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

    // Make sure the paths are given in absolutes
    try {
        if (!Output.is_absolute())
            Output = std::filesystem::absolute(Output);
    } catch (...) {
        // Ignore it
    }

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

void ProgramOptions::populate(RuntimeOptions& options) const
{
    IG_LOGGER.setQuiet(Quiet);
    IG_LOGGER.setVerbosity((DebugTrace || DumpRegistry || DumpFullRegistry) ? IG::L_DEBUG : VerbosityLevel);
    IG_LOGGER.enableAnsiTerminal(!NoColor);

    options.IsTracer      = Type == ApplicationType::Trace;
    options.IsInteractive = Type == ApplicationType::View;

    options.Target           = Target;
    options.AcquireStats     = AcquireStats || AcquireFullStats;
    options.DebugTrace       = DebugTrace;
    options.DumpShader       = DumpShader || DumpFullShader;
    options.DumpShaderFull   = DumpFullShader;
    options.DumpRegistry     = DumpRegistry || DumpFullRegistry;
    options.DumpRegistryFull = DumpFullRegistry;
    options.SPI              = SPI.value_or(0);

    options.Seed = (size_t)Seed;

    options.OverrideTechnique = TechniqueType;
    options.OverrideCamera    = CameraType;
    if (Width.has_value() && Height.has_value())
        options.OverrideFilmSize = { Width.value(), Height.value() };

    options.AddExtraEnvLight = AddExtraEnvLight;
    options.Specialization   = Specialization;

    options.Denoiser.Enabled = Denoise;

    options.Glare.Enabled = Glare;

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