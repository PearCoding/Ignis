#pragma once

#include "IG_Config.h"

namespace IG {
/// Transform string to lowercase
[[nodiscard]] IG_LIB std::string to_lowercase(const std::string& str);

/// Transform string to UPPERCASE
[[nodiscard]] IG_LIB std::string to_uppercase(const std::string& str);

/// Transform string to Uppercase
[[nodiscard]] IG_LIB std::string to_word_uppercase(const std::string& str);

/// Transform string to a string with whitespace replaced by the character given with c
[[nodiscard]] IG_LIB std::string whitespace_escaped(const std::string& str, const std::string::value_type& c = '_');
} // namespace IG
