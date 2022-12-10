#include "StringUtils.h"

namespace IG {
std::string to_lowercase(const std::string& str)
{
    std::string tmp = str;
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [](const std::string::value_type& v) { return static_cast<std::string::value_type>(::tolower((int)v)); });
    return tmp;
}

std::string to_uppercase(const std::string& str)
{
    std::string tmp = str;
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [](const std::string::value_type& v) { return static_cast<std::string::value_type>(::toupper((int)v)); });
    return tmp;
}

std::string to_word_uppercase(const std::string& str)
{
    std::string tmp = str;

    bool lastWS = true;
    for (auto& c : tmp) {
        if (std::isspace(c)) {
            lastWS = true;
        } else if (lastWS && std::islower(c)) {
            lastWS = false;
            c      = std::toupper(c);
        }
    }

    return tmp;
}

std::string whitespace_escaped(const std::string& str, const std::string::value_type& c)
{
    std::string tmp = str;
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [=](const std::string::value_type& v) { return std::isspace(v) ? c : v; });
    return tmp;
}
} // namespace IG