#pragma once

#include "IG_Config.h"

namespace IG {
struct KlemsComponentSpecification {
    std::pair<size_t, size_t> theta_count;
    std::pair<size_t, size_t> entry_count;
    float total;
};
struct KlemsSpecification {
    KlemsComponentSpecification front_reflection;
    KlemsComponentSpecification back_reflection;
    KlemsComponentSpecification front_transmission;
    KlemsComponentSpecification back_transmission;
};
class KlemsLoader {
public:
    static bool prepare(const std::filesystem::path& in_xml, const std::filesystem::path& out_data, KlemsSpecification& spec);
};
} // namespace IG