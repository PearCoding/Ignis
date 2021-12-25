#include "ConsoleLogListener.h"

// clang-format off
#define ANSI_BLACK 			"\u001b[0;30m"
#define ANSI_RED 			"\u001b[0;31m"
#define ANSI_GREEN 			"\u001b[0;32m"
#define ANSI_BROWN 			"\u001b[0;33m"
#define ANSI_BLUE 			"\u001b[0;34m"
#define ANSI_MAGENTA 		"\u001b[0;35m"
#define ANSI_CYAN 			"\u001b[0;36m"
#define ANSI_GRAY 			"\u001b[0;37m"
#define ANSI_DARK_GRAY 		"\u001b[1;30m"
#define ANSI_LIGHT_RED 		"\u001b[1;31m"
#define ANSI_LIGHT_GREEN 	"\u001b[1;32m"
#define ANSI_YELLOW 		"\u001b[1;33m"
#define ANSI_LIGHT_BLUE 	"\u001b[1;34m"
#define ANSI_LIGHT_MAGENTA 	"\u001b[1;35m"
#define ANSI_LIGHT_CYAN 	"\u001b[1;36m"
#define ANSI_WHITE 			"\u001b[1;37m"
#define ANSI_RESET 			"\u001b[0m"
// clang-format on

namespace IG {
ConsoleLogListener::ConsoleLogListener(bool useAnsi)
    : LogListener()
    , mUseAnsi(useAnsi)
{
}

ConsoleLogListener::~ConsoleLogListener()
{
}

void ConsoleLogListener::startEntry(LogLevel level)
{
    std::cout << "[";
    if (mUseAnsi) {
        switch (level) {
        case L_DEBUG:
            std::cout << ANSI_DARK_GRAY;
            break;
        case L_INFO:
            break;
        case L_WARNING:
            std::cout << ANSI_YELLOW;
            break;
        case L_ERROR:
            std::cout << ANSI_LIGHT_RED;
            break;
        case L_FATAL:
            std::cout << ANSI_RED;
            break;
        }
    }

    std::cout << Logger::levelString(level);

    if (mUseAnsi) {
        std::cout << ANSI_RESET;
    }

    std::cout << "] ";
}

void ConsoleLogListener::writeEntry(int c)
{
    std::cout.put(c);
}
} // namespace IG
