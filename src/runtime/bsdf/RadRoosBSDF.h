#pragma once

#include "BSDF.h"

namespace IG {
class RadRoosBSDF : public BSDF {
public:
    RadRoosBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf);

    void serialize(const SerializationInput& input) const override;

private:
    std::shared_ptr<SceneObject> mBSDF;
};
} // namespace IG