#include "UtilOptions.h"
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

UtilOptions::UtilOptions(int argc, char** argv, const std::string& desc)
{
    CLI::App app{ desc, argc >= 1 ? argv[0] : "unknown" };

    app.positionals_at_end(false);
    app.set_version_flag("--version", Build::getBuildString());
    app.set_help_flag("-h,--help", "Shows help message and exit");

    app.add_flag("-q,--quiet", Quiet, "Do not print messages into console");
    app.add_flag_callback(
        "-v,--verbose", [&]() { VerbosityLevel = L_DEBUG; }, "Set the verbosity level to 'debug'. Shortcut for --log-level debug");

    app.add_option("--log-level", VerbosityLevel, "Set the verbosity level")->transform(EnumValidator(LogLevelMap, CLI::ignore_case));

    app.add_flag("--no-color", NoColor, "Do not use decorations to make console output better");
    app.add_flag("--no-logo", NoLogo, "Do not use show copyright");

    app.require_subcommand(1);

    // -- Info
    auto info = app.add_subcommand("info", "Acquire information from a file");
    info->add_option("input", InputFile, "Image file to acquire information from")->check(CLI::ExistingFile);

    // -- Convert
    auto convert = app.add_subcommand("convert", "Convert from one format to another");

    convert->add_option("input", InputFile, "Input file")->check(CLI::ExistingFile);
    convert->add_option("output", OutputFile, "Output file")->check(CLI::ExistingFile);

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