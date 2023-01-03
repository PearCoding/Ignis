#pragma once

#include "IG_Config.h"
#include <string_view>

namespace IG {
/// Transform string to lowercase
[[nodiscard]] IG_LIB std::string to_lowercase(const std::string& str);

/// Transform string to UPPERCASE
[[nodiscard]] IG_LIB std::string to_uppercase(const std::string& str);

/// Transform string to Uppercase
[[nodiscard]] IG_LIB std::string to_word_uppercase(const std::string& str);

/// Transform string to a string with whitespace replaced by the character given with c
[[nodiscard]] IG_LIB std::string whitespace_escaped(const std::string& str, const std::string::value_type& c = '_');

[[nodiscard]] IG_LIB bool string_starts_with(std::string_view str, std::string_view prefix);
[[nodiscard]] IG_LIB bool string_starts_with(const std::string& str, const std::string& prefix);

[[nodiscard]] IG_LIB bool string_ends_with(std::string_view str, std::string_view suffix);
[[nodiscard]] IG_LIB bool string_ends_with(const std::string& str, const std::string& suffix);

/// Trim string from the left side in place
IG_LIB void string_left_trim(std::string& s);

/// Trim string from the right side in place
IG_LIB void string_right_trim(std::string& s);

/// Trim string from the left and right side in place
IG_LIB void string_trim(std::string& s);
} // namespace IG
