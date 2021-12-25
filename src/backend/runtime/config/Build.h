#pragma once

#include "IG_Config.h"

namespace IG {
namespace Build {
struct Version {
    uint32 Major;
    uint32 Minor;
    inline uint32 asNumber() const { return ((Major) << 8) | (Minor); }
};
/**
 * @brief Returns version of the build
 * 
 * @return Version struct containing major and minor version 
 */
Version getVersion();
/**
 * @brief Returns version of the build as a string
 * 
 * @return std::string getVersionString 
 */
std::string getVersionString();
/**
 * @brief Returns git branch and revision of the build as a string
 * 
 * @return std::string getGitString 
 */
std::string getGitString();
/**
 * @brief Returns copyright of the build as a string
 * 
 * @return std::string getCopyrightString 
 */
std::string getCopyrightString();
/**
 * @brief Returns name of compiler used for compiling
 * 
 * @return std::string getCompilerName 
 */
std::string getCompilerName();
/**
 * @brief Returns name of operating system the software was compiled with
 * 
 * @return std::string getOSName 
 */
std::string getOSName();
/**
 * @brief Returns name of variant. Most of the time Debug or Release
 * 
 * @return std::string getBuildVariant 
 */
std::string getBuildVariant();
/**
 * @brief Composed string representing all important options used while compiling the software
 * 
 * @return std::string getBuildString 
 */
std::string getBuildString();
} // namespace Build
} // namespace IG