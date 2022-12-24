#include "ConsoleLogListener.h"

namespace IG {

[[maybe_unused]] constexpr std::string_view ANSI_BLACK         = "\u001b[0;30m";
[[maybe_unused]] constexpr std::string_view ANSI_RED           = "\u001b[0;31m";
[[maybe_unused]] constexpr std::string_view ANSI_GREEN         = "\u001b[0;32m";
[[maybe_unused]] constexpr std::string_view ANSI_BROWN         = "\u001b[0;33m";
[[maybe_unused]] constexpr std::string_view ANSI_BLUE          = "\u001b[0;34m";
[[maybe_unused]] constexpr std::string_view ANSI_MAGENTA       = "\u001b[0;35m";
[[maybe_unused]] constexpr std::string_view ANSI_CYAN          = "\u001b[0;36m";
[[maybe_unused]] constexpr std::string_view ANSI_GRAY          = "\u001b[0;37m";
[[maybe_unused]] constexpr std::string_view ANSI_DARK_GRAY     = "\u001b[1;30m";
[[maybe_unused]] constexpr std::string_view ANSI_LIGHT_RED     = "\u001b[1;31m";
[[maybe_unused]] constexpr std::string_view ANSI_LIGHT_GREEN   = "\u001b[1;32m";
[[maybe_unused]] constexpr std::string_view ANSI_YELLOW        = "\u001b[1;33m";
[[maybe_unused]] constexpr std::string_view ANSI_LIGHT_BLUE    = "\u001b[1;34m";
[[maybe_unused]] constexpr std::string_view ANSI_LIGHT_MAGENTA = "\u001b[1;35m";
[[maybe_unused]] constexpr std::string_view ANSI_LIGHT_CYAN    = "\u001b[1;36m";
[[maybe_unused]] constexpr std::string_view ANSI_WHITE         = "\u001b[1;37m";
[[maybe_unused]] constexpr std::string_view ANSI_RESET         = "\u001b[0m";

ConsoleLogListener::ConsoleLogListener(bool useAnsi)
    : LogListener()
    , mUseAnsi(useAnsi)
{
}

void ConsoleLogListener::startEntry(LogLevel level)
{
    std::cout << "[";
    if (mUseAnsi) 
        startColoring(level);

    std::cout << Logger::levelString(level);

    if (mUseAnsi)
        stopColoring();

    std::cout << "] ";
}

void ConsoleLogListener::writeEntry(char c)
{
    std::cout.put(c);
}

void ConsoleLogListener::startColoring(LogLevel level) 
{
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

void ConsoleLogListener::stopColoring() 
{
    std::cout << ANSI_RESET;
}
} // namespace IG
