#pragma once

#include "BSDF.h"

namespace IG {
class ErrorBSDF : public BSDF {
public:
    ErrorBSDF(const std::string& name);

    void serialize(const SerializationInput& input) const override;

    static std::string inlineError(const std::string& name);
};
} // namespace IG