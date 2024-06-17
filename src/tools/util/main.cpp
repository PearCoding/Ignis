
#include "Logger.h"
#include "UtilOptions.h"
#include "config/Build.h"

using namespace IG;

int main(int argc, char** argv)
{
    UtilOptions cmd(argc, argv, "Ignis Utility Tool");
    if (cmd.ShouldExit)
        return EXIT_SUCCESS;

    if (!cmd.Quiet && !cmd.NoLogo)
        std::cout << Build::getCopyrightString() << std::endl;

    try {
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}