#pragma once

#include "Pattern.h"

namespace IG {
class TransformPattern : public Pattern {
public:
    TransformPattern(const std::string& name, const std::shared_ptr<SceneObject>& obj);

    void serialize(const SerializationInput& input) const override;

private:
    std::shared_ptr<SceneObject> mObject;
};
} // namespace IG