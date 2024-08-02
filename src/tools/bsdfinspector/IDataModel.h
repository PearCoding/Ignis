#pragma once

#include "IG_Config.h"

namespace IG {
enum class DataComponent {
    FrontReflection = 0,
    FrontTransmission,
    BackReflection,
    BackTransmission
};

class Colormapper;
class IDataModel {
public:
    virtual void renderView(float incidentTheta, float incidentPhi, float radius, DataComponent component, const Colormapper& mapper) = 0;
    virtual void renderInfo(float incidentTheta, float incidentPhi, float outgoingTheta, float outgoingPhi, DataComponent component)  = 0;
    virtual void renderProperties(float incidentTheta, float incidentPhi, DataComponent component)                                    = 0;
};
} // namespace IG