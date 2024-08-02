#pragma once

#include "IDataModel.h"

namespace IG {
class Klems;
class KlemsDataModel : public IDataModel {
public:
    KlemsDataModel(const std::shared_ptr<Klems>& klems);
    ~KlemsDataModel();

    void renderView(float incidentTheta, float incidentPhi, float radius, DataComponent component, const Colormapper& mapper) override;
    void renderInfo(float incidentTheta, float incidentPhi, float outgoingTheta, float outgoingPhi, DataComponent component) override;
    void renderProperties(float incidentTheta, float incidentPhi, DataComponent component) override;

private:
    std::shared_ptr<Klems> mKlems;
};
} // namespace IG