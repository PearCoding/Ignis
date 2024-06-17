#pragma once

#include "IG_Config.h"

namespace IG {
#ifdef IG_WITH_CONVERTER_MITSUBA
bool convert_from_mts(const Path& input, const Path& output, const std::vector<Path>& lookup_dirs, const std::unordered_map<std::string, std::string>& defs, bool use_ws, bool is_quiet);
#endif
} // namespace IG