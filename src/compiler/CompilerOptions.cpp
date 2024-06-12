#include "CompilerOptions.h"
#include "EnumValidator.h"
#include "Runtime.h"
#include "config/Build.h"
#include "loader/Transpiler.h"

namespace IG {
static const std::map<std::string, LogLevel> LogLevelMap{ { "fatal", L_FATAL }, { "error", L_ERROR }, { "warning", L_WARNING }, { "info", L_INFO }, { "debug", L_DEBUG } };

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

CompilerOptions::CompilerOptions(int argc, char** argv, const std::string& desc)
{
    CLI::App app{ desc, argc >= 1 ? argv[0] : "unknown" };

    app.positionals_at_end(false);
    app.set_version_flag("--version", Build::getBuildString());
    app.set_help_flag("-h,--help", "Shows help message and exit");

    app.add_option("file", InputFile, "Scene file to load. Can be a Ignis scene file or a glTF file.")->check(CLI::ExistingFile);
    app.add_flag("-q,--quiet", Quiet, "Do not print messages into console");
    app.add_flag_callback(
        "-v,--verbose", [&]() { VerbosityLevel = L_DEBUG; }, "Set the verbosity level to 'debug'. Shortcut for --log-level debug");

    app.add_option("--log-level", VerbosityLevel, "Set the verbosity level")->transform(EnumValidator(LogLevelMap, CLI::ignore_case));

    app.add_flag("--no-color", NoColor, "Do not use decorations to make console output better");
    app.add_flag("--no-logo", NoLogo, "Do not use show copyright");

    app.add_option("-O,--optimization", OptimizationLevel, "Level of optimization applied to shaders. Range is [0, 3]. Level 0 will also add debug information")->default_val(OptimizationLevel);

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
}

} // namespace IG