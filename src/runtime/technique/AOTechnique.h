#pragma once

#include "Technique.h"

namespace IG {
class AOTechnique : public Technique {
public:
    AOTechnique();
    ~AOTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;
};
} // namespace IG