#pragma once

#include "Technique.h"

namespace IG {
class WireframeTechnique : public Technique {
public:
    WireframeTechnique();
    ~WireframeTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;
};
} // namespace IG