#pragma once

#include "IDataModel.h"

namespace IG {
class TensorTree;
class TensorDataModel : public IDataModel {
public:
    TensorDataModel(const std::shared_ptr<TensorTree>& ttree);
    ~TensorDataModel();

    void renderView(float incidentTheta, float incidentPhi, float radius, DataComponent component, const Colormapper& mapper) override;
    void renderInfo(float incidentTheta, float incidentPhi, float outgoingTheta, float outgoingPhi, DataComponent component) override;
    void renderProperties(float incidentTheta, float incidentPhi, DataComponent component) override;

private:
    std::shared_ptr<TensorTree> mTensorTree;
};
} // namespace IG