#pragma once

#include "IG_Config.h"

#include <chrono>

namespace IG {
class SceneObject {
public:
    inline SceneObject(const std::string& name, const std::string& type)
        : mName(name)
        , mType(type)
    {
    }

    inline const std::string& name() const { return mName; }
    inline const std::string& type() const { return mType; }

private:
    std::string mName;
    std::string mType;
};

} // namespace IG