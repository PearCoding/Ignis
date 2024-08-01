#include "KlemsDataModel.h"
#include "measured/KlemsLoader.h"

#include "UI.h"

namespace IG {
KlemsDataModel::KlemsDataModel(const std::shared_ptr<Klems>& klems)
    : mKlems(klems)
{
}

KlemsDataModel::~KlemsDataModel()
{
}

static inline KlemsComponent* getComponent(const std::shared_ptr<Klems>& klems, DataComponent component)
{
    switch (component) {
    default:
    case DataComponent::FrontReflection:
        return klems->front_reflection.get();
    case DataComponent::FrontTransmission:
        return klems->front_transmission.get();
    case DataComponent::BackReflection:
        return klems->back_reflection.get();
    case DataComponent::BackTransmission:
        return klems->back_transmission.get();
    }
}

void KlemsDataModel::renderView(const Vector3f& view, DataComponent component)
{
}

void KlemsDataModel::renderProperties(DataComponent component)
{
    KlemsComponent* comp = getComponent(mKlems, component);
    if (!comp)
        return;

    const float total   = comp->computeTotal();
    const float maxHemi = comp->computeMaxHemisphericalScattering();
    const float minProj = comp->computeMinimumProjectedSolidAngle();

    ImGui::Text("Total:    %3.2f", total);
    ImGui::Text("Max Hemi: %3.2f", maxHemi);
    ImGui::Text("Min Proj: %3.2f", minProj);
}
} // namespace IG