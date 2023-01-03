#pragma once

#include "Pattern.h"

namespace IG {
class BrickPattern : public Pattern {
public:
    BrickPattern(const std::string& name, const std::shared_ptr<SceneObject>& obj);

    void serialize(const SerializationInput& input) const override;

private:
    std::shared_ptr<SceneObject> mObject;
};
} // namespace IG