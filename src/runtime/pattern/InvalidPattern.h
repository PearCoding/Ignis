#pragma once

#include "Pattern.h"

namespace IG {
class InvalidPattern : public Pattern {
public:
    InvalidPattern(const std::string& name);

    void serialize(const SerializationInput& input) const override;

    static std::string inlineError(const std::string& name);
};
} // namespace IG