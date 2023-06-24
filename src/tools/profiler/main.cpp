#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "Logger.h"
#include "UI.h"
#include "statistics/Statistics.h"

using namespace IG;

static inline void check_arg(int argc, char** argv, int arg, int n)
{
    if (arg + n >= argc)
        IG_LOG(L_ERROR) << "Option '" << argv[arg] << "' expects " << n << " arguments, got " << (argc - arg) << std::endl;
}

static inline void version()
{
    std::cout << "ig_profiler 0.1" << std::endl;
}

static inline void usage()
{
    std::cout
        << "ig_profiler - Tool to analyse Ignis profile output" << std::endl
        << "Usage: ig_profiler [options] file" << std::endl
        << "Available options:" << std::endl
        << "   -h      --help                   Shows this message" << std::endl
        << "           --version                Show version and exit" << std::endl
        << "   -q      --quiet                  Do not print messages into console" << std::endl
        << "   -v      --verbose                Print detailed information" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc <= 1) {
        usage();
        return EXIT_SUCCESS;
    }

    Path in_file;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet")) {
                IG_LOGGER.setQuiet(true);
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                IG_LOGGER.setVerbosity(L_DEBUG);
            } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                usage();
                return EXIT_SUCCESS;
            } else if (!strcmp(argv[i], "--version")) {
                version();
                return EXIT_SUCCESS;
            } else {
                IG_LOG(L_ERROR) << "Unknown option '" << argv[i] << "'" << std::endl;
                usage();
                return EXIT_FAILURE;
            }
        } else {
            if (in_file.empty()) {
                in_file = argv[i];
            } else {
                IG_LOG(L_ERROR) << "Unexpected argument '" << argv[i] << "'" << std::endl;
                usage();
                return EXIT_FAILURE;
            }
        }
    }

    // Check some stuff
    if (in_file.empty()) {
        IG_LOG(L_ERROR) << "No input file given" << std::endl;
        return EXIT_FAILURE;
    }

    std::string json_file;
    {
        std::ifstream stream(in_file);
        std::stringstream buffer;
        buffer << stream.rdbuf();
        json_file = buffer.str();
    }

    Statistics stats;
    float total_ms = 0;
    if (!stats.loadFromJSON(json_file, &total_ms)) {
        IG_LOG(L_ERROR) << "Given input file is invalid." << std::endl;
        return EXIT_FAILURE;
    }

    // Display ui
    std::unique_ptr<UI> ui;
    try {
        ui = std::make_unique<UI>(stats, total_ms);
    } catch (...) {
        return EXIT_FAILURE;
    }

    while (ui->handleInput() != UI::InputResult::Quit) {
        ui->update();
    }
    return EXIT_SUCCESS;
}