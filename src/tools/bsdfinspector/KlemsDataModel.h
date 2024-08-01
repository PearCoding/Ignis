#pragma once

#include "IDataModel.h"

namespace IG {
class Klems;
class KlemsDataModel : public IDataModel {
public:
    KlemsDataModel(const std::shared_ptr<Klems>& klems);
    ~KlemsDataModel();

    void renderView(const Vector3f& view, DataComponent component) override;
    void renderProperties(DataComponent component) override;

private:
    std::shared_ptr<Klems> mKlems;
};
} // namespace IG