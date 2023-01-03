#pragma once

#include "BSDF.h"

namespace IG {
class PlasticBSDF : public BSDF {
public:
    PlasticBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf);

    void serialize(const SerializationInput& input) const override;

private:
    std::shared_ptr<SceneObject> mBSDF;
};
} // namespace IG