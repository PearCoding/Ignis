#pragma once

#include "IG_Config.h"

namespace IG {
class KlemsLoader {
public:
	static bool prepare(const std::string& in_xml, const std::string& out_data);
};
} // namespace IG