#pragma once

#include "BSDF.h"

namespace IG {
class MaskBSDF : public BSDF {
public:
    enum class Type {
        Mask,
        Cutoff
    };
    MaskBSDF(Type type, const std::string& name, const std::shared_ptr<SceneObject>& bsdf);

    void serialize(const SerializationInput& input) const override;

private:
    std::shared_ptr<SceneObject> mBSDF;
    Type mType;
};
} // namespace IG