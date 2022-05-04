#pragma once

#include "IG_Config.h"

namespace IG {
struct TensorTreeComponentSpecification {
    size_t node_count;
    size_t value_count;
    float total;
    bool root_is_leaf;
};
struct TensorTreeSpecification {
    size_t ndim;
    TensorTreeComponentSpecification front_reflection;
    TensorTreeComponentSpecification back_reflection;
    TensorTreeComponentSpecification front_transmission;
    TensorTreeComponentSpecification back_transmission;
};

class TensorTreeLoader {
public:
    static bool prepare(const std::filesystem::path& in_xml, const std::filesystem::path& out_data, TensorTreeSpecification& spec);
};
} // namespace IG