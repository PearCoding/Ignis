#pragma once

#include "Parser.h"

namespace IG {
namespace Parser {
// --------------- glTFSceneParser
class glTFSceneParser {
public:
    static Scene loadFromFile(const std::string& path, bool& ok);
};
} // namespace Parser
} // namespace IG