#include "UtilOptions.h"
#include "EnumValidator.h"
#include "Runtime.h"
#include "StringUtils.h"
#include "config/Build.h"

namespace IG {
static const std::map<std::string, LogLevel> LogLevelMap{ { "fatal", L_FATAL }, { "error", L_ERROR }, { "warning", L_WARNING }, { "info", L_INFO }, { "debug", L_DEBUG } };
static const std::map<std::string, ConvertOption> ConvertOptionMap{
    { "auto", ConvertOption::Automatic },
    { "bmp", ConvertOption::BMP },
    { "exr", ConvertOption::EXR },
    { "hdr", ConvertOption::HDR },
#ifdef IG_WITH_CONVERTER_MITSUBA
    { "ignis", ConvertOption::IG },
#endif
    { "jpg", ConvertOption::JPG },
    { "obj", ConvertOption::OBJ },
    { "ply", ConvertOption::PLY },
    { "png", ConvertOption::PNG },
    { "tga", ConvertOption::TGA },
};

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
    info->add_option("input", InputFile, "Image file to acquire information from")->check(CLI::ExistingFile)->required();
    info->callback([&]() { Op = Operation::Info; });

    // -- Convert
    auto convert = app.add_subcommand("convert", "Convert from one format to another");

    convert->add_option("input", InputFile, "Input file")->check(CLI::ExistingFile)->required();
    convert->add_option("output", OutputFile, "Output file")->check(CLI::ExistingFile)->required();
    convert->add_option("--format", ConvertFormat, "Format to convert to. If auto, format will be determined by the output path name")->transform(EnumValidator(ConvertOptionMap, CLI::ignore_case))->default_str("auto");

    convert->add_option("--exposure", ToneMapExposure, "Set tonemap exposure for output image");
    convert->add_option("--offset", ToneMapOffset, "Set tonemap offset for output image");
    convert->add_option("--jpg-quality", JPGQuality, "Quality between [0, 100] for jpg output");

#ifdef IG_WITH_CONVERTER_MITSUBA
    convert->add_option("--mts-lookup", MtsLookupDirs, "Directory to add for lookup during mitsuba scene file parsing");
    convert->add_option("--mts-define", MtsDefs, "Definitions to use during mitsuba scene file parsing");
#endif

    convert->callback([&]() { Op = Operation::Convert; });

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

    if (Op == Operation::Convert) {
        if (ConvertFormat == ConvertOption::Automatic) {
            using namespace std::string_view_literals;
            std::string ext = to_lowercase(OutputFile.extension().generic_string());
            if (ext == ".jpg"sv || ext == ".jpeg"sv)
                ConvertFormat = ConvertOption::JPG;
            else if (ext == ".png"sv)
                ConvertFormat = ConvertOption::PNG;
            else if (ext == ".bmp"sv)
                ConvertFormat = ConvertOption::BMP;
            else if (ext == ".tga"sv)
                ConvertFormat = ConvertOption::TGA;
            else if (ext == ".hdr"sv)
                ConvertFormat = ConvertOption::HDR;
            else if (ext == ".exr"sv)
                ConvertFormat = ConvertOption::EXR;
            else if (ext == ".obj"sv)
                ConvertFormat = ConvertOption::OBJ;
            else if (ext == ".ply"sv)
                ConvertFormat = ConvertOption::PNG;
#ifdef IG_WITH_CONVERTER_MITSUBA
            else if (ext == ".json"sv)
                ConvertFormat = ConvertOption::IG;
#endif
            else {
                std::cerr << "Can not determine format to convert to from the given output file " << OutputFile << std::endl;
                ShouldExit = true;
                return;
            }
        }
    }
}

} // namespace IG