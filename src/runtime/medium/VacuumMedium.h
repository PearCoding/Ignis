#pragma once

#include "Medium.h"

namespace IG {
class VacuumMedium : public Medium {
public:
    VacuumMedium(const std::string& name);
    void serialize(const SerializationInput& input) const override;
};
} // namespace IG