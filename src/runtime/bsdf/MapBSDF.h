#pragma once

#include "BSDF.h"

namespace IG {
class MapBSDF : public BSDF {
public:
    enum class Type {
        Normal,
        Bump
    };
    MapBSDF(Type type, const std::string& name, const std::shared_ptr<SceneObject>& bsdf);

    void serialize(const SerializationInput& input) const override;

private:
    std::shared_ptr<SceneObject> mBSDF;
    Type mType;
};
} // namespace IG