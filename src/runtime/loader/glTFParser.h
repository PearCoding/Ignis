#pragma once

#include "Parser.h"

namespace IG {
class glTFSceneParser {
public:
    static std::shared_ptr<Scene> loadFromFile(const Path& path);
};
} // namespace IG