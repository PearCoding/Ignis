#pragma once

#include "DebugMode.h"
#include "Technique.h"

namespace IG {
class DebugTechnique : public Technique {
public:
    DebugTechnique(SceneObject& obj);
    ~DebugTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

private:
    DebugMode mInitialDebugMode;
};
} // namespace IG