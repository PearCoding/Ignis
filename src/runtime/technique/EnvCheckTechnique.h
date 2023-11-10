#pragma once

#include "Technique.h"

namespace IG {
class EnvCheckTechnique : public Technique {
public:
    EnvCheckTechnique();
    ~EnvCheckTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;
};
} // namespace IG