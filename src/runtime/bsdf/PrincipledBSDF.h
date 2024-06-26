#pragma once

#include "BSDF.h"

namespace IG {
class PrincipledBSDF : public BSDF {
public:
    PrincipledBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf);

    void serialize(const SerializationInput& input) const override;

private:
    std::shared_ptr<SceneObject> mBSDF;
    bool mUseExplicitRoughness;
};
} // namespace IG