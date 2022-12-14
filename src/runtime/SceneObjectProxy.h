#pragma once

#include "IG_Config.h"

#include <chrono>

namespace IG {
class SceneObjectProxy {
public:
    inline SceneObjectProxy(const std::string& name, const std::string& type)
        : mName(name)
        , mType(type)
    {
    }

    [[nodiscard]] inline const std::string& name() const { return mName; }
    [[nodiscard]] inline const std::string& type() const { return mType; }

private:
    std::string mName;
    std::string mType;
};

} // namespace IG