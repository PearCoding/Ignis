#pragma once

#include "Technique.h"

namespace IG {
class DebugTechnique : public Technique {
public:
    DebugTechnique();
    ~DebugTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;
};
} // namespace IG