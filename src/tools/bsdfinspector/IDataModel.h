#pragma once

#include "IG_Config.h"

namespace IG {
enum class DataComponent {
    FrontReflection = 0,
    FrontTransmission,
    BackReflection,
    BackTransmission
};

class IDataModel {
public:
    virtual void renderView(const Vector3f& view, DataComponent component) = 0;
    virtual void renderProperties(DataComponent component)                 = 0;
};
} // namespace IG