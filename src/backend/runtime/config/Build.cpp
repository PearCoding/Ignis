#include "Build.h"
#include "config/Git.h"
#include "config/Version.h"

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
std::string getGitString() { return IG_GIT_BRANCH " " IG_GIT_REVISION; }
std::string getCopyrightString() { return "(C) " IG_VENDOR_STRING; }

std::string getCompilerName() { return IG_CC_NAME; }
std::string getOSName() { return IG_OS_NAME; }
std::string getBuildVariant() { return IG_BUILDVARIANT_NAME; }

std::string getBuildString()
{
#ifdef IG_NO_ASSERTS
    constexpr bool hasAsserts = false;
#else
    constexpr bool hasAsserts = true;
#endif

    std::stringstream stream;
    stream << std::boolalpha
           << IG_NAME_STRING << " " << IG_VERSION_STRING
           << " (" << getBuildVariant()
           << ") on " __DATE__ " at " __TIME__
           << " with " << getCompilerName()
           << " { OS: " << getOSName()
           << "; Branch: " IG_GIT_BRANCH
           << "; Rev: " IG_GIT_REVISION
           << "; Date: " IG_GIT_DATE
           << "; Subject: " IG_GIT_SUBJECT
           << "} [Asserts: " << hasAsserts
           << "]";
    if (IG_GIT_DIRTY)
        stream << " [Dirty]";

    return stream.str();
}
} // namespace IG::Build