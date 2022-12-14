#pragma once

#include "Technique.h"

namespace IG {
class CameraCheckTechnique : public Technique {
public:
    CameraCheckTechnique();
    ~CameraCheckTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;
};
} // namespace IG