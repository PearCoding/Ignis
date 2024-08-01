#pragma once

#include "IDataModel.h"

namespace IG {
class Klems;
class KlemsDataModel : public IDataModel {
public:
    KlemsDataModel(const std::shared_ptr<Klems>& klems);
    ~KlemsDataModel();

    void renderView(float viewTheta, float viewPhi, float radius, DataComponent component) override;
    void renderProperties(float viewTheta, float viewPhi, DataComponent component) override;

private:
    std::shared_ptr<Klems> mKlems;
};
} // namespace IG