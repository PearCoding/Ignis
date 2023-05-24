#pragma once

#include "Pattern.h"

namespace IG {
class ExprPattern : public Pattern {
public:
    ExprPattern(const std::string& name, const std::shared_ptr<SceneObject>& obj);

    void serialize(const SerializationInput& input) const override;
    std::pair<size_t, size_t> computeResolution(ShadingTree&) const override;

private:
    std::shared_ptr<SceneObject> mObject;
};
} // namespace IG