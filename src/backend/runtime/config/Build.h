#pragma once

#include "IG_Config.h"

namespace IG::Build {
struct Version {
    uint32 Major;
    uint32 Minor;
    [[nodiscard]] inline uint32 asNumber() const { return ((Major) << 8) | (Minor); }
};
/**
 * @brief Returns version of the build
 *
 * @return Version struct containing major and minor version
 */
[[nodiscard]] Version getVersion();
/**
 * @brief Returns version of the build as a string
 *
 * @return std::string getVersionString
 */
[[nodiscard]] std::string getVersionString();
/**
 * @brief Returns git revision of the build as a string
 *
 * @return std::string getGitRevision
 */
[[nodiscard]] std::string getGitRevision();
/**
 * @brief Returns git branch and revision of the build as a string
 *
 * @return std::string getGitString
 */
[[nodiscard]] std::string getGitString();
/**
 * @brief Returns copyright of the build as a string
 *
 * @return std::string getCopyrightString
 */
[[nodiscard]] std::string getCopyrightString();
/**
 * @brief Returns name of compiler used for compiling
 *
 * @return std::string getCompilerName
 */
[[nodiscard]] std::string getCompilerName();
/**
 * @brief Returns name of operating system the software was compiled with
 *
 * @return std::string getOSName
 */
[[nodiscard]] std::string getOSName();
/**
 * @brief Returns name of variant. Most of the time Debug or Release
 *
 * @return std::string getBuildVariant
 */
[[nodiscard]] std::string getBuildVariant();
/**
 * @brief Composed string representing all important options used while compiling the software
 *
 * @return std::string getBuildString
 */
[[nodiscard]] std::string getBuildString();
} // namespace IG::Build