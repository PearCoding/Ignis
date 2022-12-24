#pragma once

#include "BSDF.h"

namespace IG {
class KlemsBSDF : public BSDF {
public:
    KlemsBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf);

    void serialize(const SerializationInput& input) const override;

private:
    std::shared_ptr<SceneObject> mBSDF;
};
} // namespace IG