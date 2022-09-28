#pragma once

#include "Parser.h"

namespace IG {
namespace Parser {
// --------------- glTFSceneParser
class glTFSceneParser {
public:
    static Scene loadFromFile(const std::filesystem::path& path, bool& ok);
};
} // namespace Parser
} // namespace IG