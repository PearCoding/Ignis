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
    virtual void renderView(float viewTheta, float viewPhi, float radius, DataComponent component) = 0;
    virtual void renderProperties(float viewTheta, float viewPhi, DataComponent component)         = 0;
};
} // namespace IG