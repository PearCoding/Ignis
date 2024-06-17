#pragma once

#include "Logger.h"

namespace IG {
enum class Operation {
    Convert,
    Info
};

enum class ConvertOption {
    Automatic,
    JPG,
    PNG,
    BMP,
    TGA,
    HDR,
    EXR,
    PLY,
    OBJ,
    IG,
};

class UtilOptions {
public:
    UtilOptions(int argc, char** argv, const std::string& desc);

    // If true, program options already handled standard stuff. Just exit the application
    bool ShouldExit = false;

    bool Quiet              = false;
    LogLevel VerbosityLevel = L_INFO;
    bool NoColor            = false;
    bool NoLogo             = false;

    Path InputFile;
    Path OutputFile;

    Operation Op                = Operation::Convert;
    ConvertOption ConvertFormat = ConvertOption::Automatic;

    float ToneMapExposure = 0;
    float ToneMapOffset   = 0;

    int JPGQuality = 90;

    std::vector<Path> MtsLookupDirs;
    std::unordered_map<std::string, std::string> MtsDefs;
};
} // namespace IG