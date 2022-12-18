#pragma once

#include "BSDF.h"

namespace IG {
class ConductorBSDF : public BSDF {
public:
    ConductorBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf);

    void serialize(const SerializationInput& input) const override;

private:
    std::shared_ptr<SceneObject> mBSDF;
};
} // namespace IG