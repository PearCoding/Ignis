#pragma once

#include "Parser.h"

namespace IG {
class glTFSceneParser {
public:
    static Scene loadFromFile(const std::filesystem::path& path, bool& ok);
};
} // namespace IG