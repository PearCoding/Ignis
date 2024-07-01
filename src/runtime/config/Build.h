#pragma once

#include "IG_Config.h"

namespace IG::Build {
struct Version {
    uint32 Major;
    uint32 Minor;
    [[nodiscard]] inline uint32 asNumber() const { return ((Major) << 8) | (Minor); }
};

inline bool operator==(const Version& a, const Version& b) { return a.asNumber() == b.asNumber(); }
inline bool operator!=(const Version& a, const Version& b) { return !(a == b); }

/**
 * @brief Returns version of the build
 *
 * @return Version struct containing major and minor version
 */
[[nodiscard]] IG_LIB Version getVersion();
/**
 * @brief Returns version of the build as a string
 *
 * @return std::string getVersionString
 */
[[nodiscard]] IG_LIB std::string getVersionString();
/**
 * @brief Returns git revision of the build as a string
 *
 * @return std::string getGitRevision
 */
[[nodiscard]] IG_LIB std::string getGitRevision();
/**
 * @brief Returns git branch of the build as a string
 *
 * @return std::string getGitBranch
 */
[[nodiscard]] IG_LIB std::string getGitBranch();
/**
 * @brief Returns time of the commit as a string
 *
 * @return std::string getGitTimeOfCommit
 */
[[nodiscard]] IG_LIB std::string getGitTimeOfCommit();
/**
 * @brief Returns true if the build repository was dirty (had uncommitted changes)
 *
 * @return bool isGitDirty
 */
[[nodiscard]] IG_LIB bool isGitDirty();
/**
 * @brief Returns string with all modified files during build
 *
 * @return std::string getGitModifiedFiles
 */
[[nodiscard]] IG_LIB std::string getGitModifiedFiles();
/**
 * @brief Returns git branch and revision of the build as a string
 *
 * @return std::string getGitString
 */
[[nodiscard]] IG_LIB std::string getGitString();
/**
 * @brief Returns copyright of the build as a string
 *
 * @return std::string getCopyrightString
 */
[[nodiscard]] IG_LIB std::string getCopyrightString();
/**
 * @brief Returns name of compiler used for compiling
 *
 * @return std::string getCompilerName
 */
[[nodiscard]] IG_LIB std::string getCompilerName();
/**
 * @brief Returns name of operating system the software was compiled with
 *
 * @return std::string getOSName
 */
[[nodiscard]] IG_LIB std::string getOSName();
/**
 * @brief Returns name of variant. Most of the time Debug or Release
 *
 * @return std::string getBuildVariant
 */
[[nodiscard]] IG_LIB std::string getBuildVariant();
/**
 * @brief Returns time of build.
 *
 * @return std::string getBuildTime
 */
[[nodiscard]] IG_LIB std::string getBuildTime();
/**
 * @brief Composed string representing all important options used while compiling the software
 *
 * @return std::string getBuildString
 */
[[nodiscard]] IG_LIB std::string getBuildString();
/**
 * @brief Composed string with all definitions defined during build
 *
 * @return std::string getBuildDefinitions
 */
[[nodiscard]] IG_LIB std::string getBuildDefinitions();
/**
 * @brief Composed string with all flags defined during build
 *
 * @return std::string getBuildOptions
 */
[[nodiscard]] IG_LIB std::string getBuildOptions();
} // namespace IG::Build