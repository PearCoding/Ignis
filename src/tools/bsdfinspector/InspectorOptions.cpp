#include "InspectorOptions.h"
#include "EnumValidator.h"
#include "Runtime.h"
#include "config/Build.h"

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

InspectorOptions::InspectorOptions(int argc, char** argv, const std::string& desc)
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

} // namespace IG