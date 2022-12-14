#include "Build.h"
#include "config/Git.h"
#include "config/Version.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>

namespace IG::Build {
// OS
#if defined(IG_OS_LINUX)
constexpr const char* const IG_OS_NAME = "Linux";
#elif defined(IG_OS_WINDOWS_32)
constexpr const char* const IG_OS_NAME           = "Microsoft Windows 32 Bit";
#elif defined(IG_OS_WINDOWS_64)
constexpr const char* const IG_OS_NAME = "Microsoft Windows 64 Bit";
#else
constexpr const char* const IG_OS_NAME = "Unknown";
#endif

// Compiler
#if defined(IG_CC_CYGWIN)
constexpr const char* const IG_CC_NAME = "Cygwin";
#elif defined(IG_CC_MINGW32)
#if !defined(IG_CC_GNU)
constexpr const char* const IG_CC_NAME           = "MinGW 32";
#else
constexpr const char* const IG_CC_NAME = "GNU C/C++(MinGW 32)";
#endif
#elif defined(IG_CC_GNU)
constexpr const char* const IG_CC_NAME = "GNU C/C++";
#elif defined(IG_CC_INTEL)
constexpr const char* const IG_CC_NAME = "Intel C/C++"
#elif defined(IG_CC_MSC)
constexpr const char* const IG_CC_NAME = "Microsoft Visual C++";
#else
constexpr const char* const IG_CC_NAME = "Unknown";
#endif

#if defined(IG_DEBUG)
constexpr const char* const IG_BUILDVARIANT_NAME = "Debug";
#else
constexpr const char* const IG_BUILDVARIANT_NAME = "Release";
#endif

Version getVersion()
{
    return Version{ IG_VERSION_MAJOR, IG_VERSION_MINOR };
}
std::string getVersionString() { return IG_VERSION_STRING; }
std::string getGitRevision() { return IG_GIT_REVISION; }
std::string getGitString() { return IG_GIT_BRANCH " " IG_GIT_REVISION; }
std::string getCopyrightString() { return "(C) " IG_VENDOR_STRING; }

std::string getCompilerName() { return IG_CC_NAME; }
std::string getOSName() { return IG_OS_NAME; }
std::string getBuildVariant() { return IG_BUILDVARIANT_NAME; }

static inline time_t parse_preprocessor(char const* date, char const* time)
{
    char s_month[4];
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    int month, day, year;
    if (std::sscanf(date, "%3s %d %d", s_month, &day, &year) != 3)
        return -1;
    s_month[3] = 0;

    int hour, minute, seconds;
    if (std::sscanf(time, "%d:%d:%d", &hour, &minute, &seconds) != 3)
        return -1;

    month = int(std::strstr(month_names, s_month) - month_names) / 3;

    struct tm tt;
    std::memset(&tt, 0, sizeof(tt));

    tt.tm_mon   = month;
    tt.tm_mday  = day;
    tt.tm_year  = year - 1900;
    tt.tm_hour  = hour;
    tt.tm_min   = minute;
    tt.tm_sec   = seconds;
    tt.tm_isdst = -1;

    return std::mktime(&tt);
}

static inline time_t parse_iso8601utc(const char* date)
{
    struct tm tt;
    std::memset(&tt, 0, sizeof(tt));

    double seconds;
    if (std::sscanf(date, "%04d-%02d-%02d %02d:%02d:%lf",
                    &tt.tm_year, &tt.tm_mon, &tt.tm_mday,
                    &tt.tm_hour, &tt.tm_min, &seconds)
        != 6)
        return -1;

    tt.tm_sec = (int)seconds;
    tt.tm_mon -= 1;
    tt.tm_year -= 1900;
    tt.tm_isdst = -1;
    return std::mktime(&tt);
}

static inline std::string build_time_str(const time_t time)
{
    std::tm* ptm = std::localtime(&time);
    char buffer[32];
    std::strftime(buffer, 32, "%a %d.%m.%Y %H:%M:%S", ptm);
    return std::string(buffer);
}

std::string getBuildString()
{
#ifdef IG_NO_ASSERTS
    constexpr bool hasAsserts = false;
#else
    constexpr bool hasAsserts = true;
#endif

    const auto compile_time = parse_preprocessor(__DATE__, __TIME__);
    const auto commit_time  = parse_iso8601utc(IG_GIT_DATE);

    std::stringstream stream;
    stream << std::boolalpha
           << IG_NAME_STRING << " " << IG_VERSION_STRING
           << " (" << getBuildVariant()
           << ") built " << build_time_str(compile_time)
           << " with " << getCompilerName()
           << " { OS: " << getOSName()
           << "; Author: " IG_GIT_AUTHOR
           << "; Branch: " IG_GIT_BRANCH
           << "; Rev: " IG_GIT_REVISION
           << "; Date: " << build_time_str(commit_time)
           << "; Subject: \"" IG_GIT_SUBJECT "\""
           << "} [Asserts: " << hasAsserts
           << "]";

    if (IG_GIT_DIRTY)
        stream << " [Dirty]";

    return stream.str();
}
} // namespace IG::Build