#pragma once

#include "Scene.h"

namespace IG {

class IG_LIB SceneParser {
    friend class InternalSceneParser;

public:
    enum Flags : uint32 {
        F_LoadCamera    = 0x1,
        F_LoadFilm      = 0x2,
        F_LoadTechnique = 0x4,

        F_LoadBSDFs    = 0x010,
        F_LoadTextures = 0x020,
        F_LoadLights   = 0x040,
        F_LoadMedia    = 0x080,
        F_LoadShapes   = 0x100,
        F_LoadEntities = 0x200,

        F_LoadExternals = 0x1000,

        F_LoadAll = F_LoadCamera | F_LoadFilm | F_LoadTechnique | F_LoadBSDFs | F_LoadTextures | F_LoadLights | F_LoadMedia | F_LoadShapes | F_LoadEntities | F_LoadExternals
    };

    inline SceneParser() = default;

    /// @brief Load from file, lookup directory will be the directory the file is in
    /// @param path Path to file containing scene description
    /// @return Scene, can be nullptr if error happened while loading
    std::shared_ptr<Scene> loadFromFile(const std::filesystem::path& path, uint32 flags = F_LoadAll);

    /// @brief Load from string, lookup directory can be set optionally, else external files depend on the current working directory
    /// @param str String containing scene description
    /// @param opt_dir Optional directory containing external files if not given as absolute files inside the scene description
    /// @return Scene, can be nullptr if error happened while loading
    std::shared_ptr<Scene> loadFromString(const std::string& str, const std::filesystem::path& opt_dir, uint32 flags = F_LoadAll);

    inline void addArgument(const std::string& key, const std::string& value)
    {
        mArguments[key] = value;
    }

private:
    std::unordered_map<std::string, std::string> mArguments;
};
} // namespace IG