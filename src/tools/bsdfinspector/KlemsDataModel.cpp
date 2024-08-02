#include "KlemsDataModel.h"
#include "Colormapper.h"
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

static inline int computeIndex(KlemsBasis* basis, float theta, float phi)
{
    phi = std::fmod(phi, 2 * Pi);

    int ind = 0;
    for (const auto& base : basis->thetaBasis()) {
        if (theta < base.UpperTheta) {
            // Find phi
            ind += std::min(base.PhiCount - 1, (uint32)std::floor(base.PhiCount * phi / (2 * Pi)));
            break;
        } else {
            ind += base.PhiCount;
        }
    }
    return ind;
}

void KlemsDataModel::renderView(float incidentTheta, float incidentPhi, float radius, DataComponent component, const Colormapper& mapper)
{
    KlemsComponent* comp = getComponent(mKlems, component);
    if (!comp)
        return;

    const ImVec2 area   = ImGui::GetContentRegionAvail();
    const ImVec2 center = ImVec2(ImGui::GetCursorScreenPos().x + area.x / 2, ImGui::GetCursorScreenPos().y + area.y / 2);

    auto& drawList = *ImGui::GetWindowDrawList();

    const auto& colThetas = comp->column()->thetaBasis();

    // Get row of view vector
    const int row = computeIndex(comp->row().get(), incidentTheta, incidentPhi);

    // const float maxOfRow = comp->matrix().row(row).maxCoeff();

    const auto radiusFromTheta = [&](float theta) {
        return radius * theta / Pi2;
    };

    const auto transform = [&](float theta, float phi) {
        const float r = radiusFromTheta(theta);
        const float x = std::cos(phi);
        const float y = std::sin(phi);
        return ImVec2(r * x + center.x, r * y + center.y);
    };

    // Draw patches
    int col = 0;
    for (const auto& base : colThetas) {
        const float theta0 = base.LowerTheta;
        const float theta1 = base.UpperTheta;
        const float scale  = base.PhiSolidAngle / Pi;

        for (uint32 phiInd = 0; phiInd < base.PhiCount; ++phiInd) {
            const float value     = comp->matrix()(row, col) / scale;
            const Vector4f mapped = mapper.apply(value);
            const ImColor color   = ImColor(mapped.x(), mapped.y(), mapped.z());

            if (base.PhiCount == 1) { // Circle
                drawList.AddCircleFilled(center, radiusFromTheta(theta1), color);
            } else {
                const float phi0 = phiInd / (float)base.PhiCount * 2 * Pi + Pi;
                const float phi1 = (phiInd + 1) / (float)base.PhiCount * 2 * Pi + Pi;

                const int patches = std::max(1, (int)std::floor(Pi / (phi1 - phi0)));

                for (int i = 0; i < patches; ++i) {
                    const float t0 = i / (float)patches;
                    const float t1 = (i + 1) / (float)patches;
                    const float p0 = phi0 * (1 - t0) + phi1 * t0;
                    const float p1 = phi0 * (1 - t1) + phi1 * t1;

                    drawList.AddQuadFilled(transform(theta0, p0), transform(theta0, p1), transform(theta1, p1), transform(theta1, p0), color);
                }
            }
            ++col;
        }
    }

    // Draw lines
    static const ImColor LineColor = ImColor(200, 200, 200);
    for (const auto& base : comp->column()->thetaBasis()) {
        const float theta0 = base.LowerTheta;
        const float theta1 = base.UpperTheta;

        const int segments = (int)std::ceil(Pi2 / theta1 * 64);
        drawList.AddCircle(center, radiusFromTheta(theta1), LineColor, segments);
        if (base.PhiCount == 1)
            continue;

        for (uint32 phiInd = 0; phiInd < base.PhiCount; ++phiInd) {
            const float phi0 = phiInd / (float)base.PhiCount * 2 * Pi + Pi;
            drawList.AddLine(transform(theta0, phi0), transform(theta1, phi0), LineColor);
        }
    }

    // Draw view point
    drawList.AddCircleFilled(transform(incidentTheta, incidentPhi), 8, ImColor(0, 100, 200));
}

void KlemsDataModel::renderInfo(float incidentTheta, float incidentPhi, float outgoingTheta, float outgoingPhi, DataComponent component)
{
    KlemsComponent* comp = getComponent(mKlems, component);
    if (!comp)
        return;

    // Get row of view vector
    const int row = computeIndex(comp->row().get(), incidentTheta, incidentPhi);
    const int col = computeIndex(comp->column().get(), outgoingTheta, outgoingPhi + Pi);

    const float value = comp->matrix()(row, col);

    float solidAngle = 0;
    for (const auto& base : comp->column()->thetaBasis()) {
        const float theta0 = base.LowerTheta;
        const float theta1 = base.UpperTheta;

        if (outgoingTheta >= theta0 && outgoingTheta <= theta1) {
            solidAngle = base.PhiSolidAngle;
            break;
        }
    }

    if (ImGui::BeginTooltip()) {
        ImGui::Text("Value:      %3.2f", value);
        ImGui::Text("SolidAngle: %3.2f", solidAngle);
        ImGui::Separator();
        ImGui::Text("Row:        %i", row);
        ImGui::Text("Column:     %i", col);
        ImGui::EndTooltip();
    }
}

void KlemsDataModel::renderProperties(float incidentTheta, float incidentPhi, DataComponent component)
{
    KlemsComponent* comp = getComponent(mKlems, component);
    if (!comp)
        return;

    // Get row of view vector
    const int row = computeIndex(comp->row().get(), incidentTheta, incidentPhi);

    const float total   = comp->computeTotal();
    const float maxHemi = comp->computeMaxHemisphericalScattering();
    const float minProj = comp->computeMinimumProjectedSolidAngle();

    ImGui::Text("Incident Patch: %i", row);

    ImGui::Text("Total:    %3.2f", total);
    ImGui::Text("Max Hemi: %3.2f", maxHemi);
    ImGui::Text("Min Proj: %3.2f", minProj);
}
} // namespace IG