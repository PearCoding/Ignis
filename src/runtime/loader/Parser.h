#pragma once

#include "Scene.h"

namespace IG {

// --------------- SceneParser
class SceneParser {
    friend class InternalSceneParser;

public:
    inline SceneParser() = default;

    /// @brief Load from file, lookup directory will be the directory the file is in
    /// @param path Path to file containing scene description
    /// @param ok True if everything is fine
    /// @return Scene, only valid if ok equals true
    Scene loadFromFile(const std::filesystem::path& path, bool& ok);

    /// @brief Load from string, lookup directory can be set optionally, else external files depend on the current working directory
    /// @param str String containing scene description
    /// @param opt_dir Optional directory containing external files if not given as absolute files inside the scene description
    /// @param ok True if everything is fine
    /// @return Scene, only valid if ok equals true
    Scene loadFromString(const std::string& str, const std::filesystem::path& opt_dir, bool& ok);

    inline void addArgument(const std::string& key, const std::string& value)
    {
        mArguments[key] = value;
    }

private:
    std::unordered_map<std::string, std::string> mArguments;
};
} // namespace IG