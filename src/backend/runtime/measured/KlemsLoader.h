#pragma once

#include "IG_Config.h"

namespace IG {
class KlemsLoader {
public:
    static bool prepare(const std::filesystem::path& in_xml, const std::filesystem::path& out_data);
};
} // namespace IG