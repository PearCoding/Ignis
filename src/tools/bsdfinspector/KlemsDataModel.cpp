#include "KlemsDataModel.h"
#include "Colormap.h"
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

void KlemsDataModel::renderView(float viewTheta, float viewPhi, float radius, DataComponent component)
{
    KlemsComponent* comp = getComponent(mKlems, component);
    if (!comp)
        return;

    const ImVec2 area   = ImGui::GetContentRegionAvail();
    const ImVec2 center = ImVec2(ImGui::GetCursorScreenPos().x + area.x / 2, ImGui::GetCursorScreenPos().y + area.y / 2);

    auto& drawList = *ImGui::GetWindowDrawList();

    const auto& rowThetas = comp->row()->thetaBasis();
    const auto& colThetas = comp->column()->thetaBasis();

    // Get row of view vector
    int row = 0;
    for (const auto& base : rowThetas) {
        if (viewTheta < base.UpperTheta) {
            // Find phi
            row += std::min(base.PhiCount - 1, (uint32)std::floor(base.PhiCount * viewPhi / (2 * Pi)));
            break;
        } else {
            row += base.PhiCount;
        }
    }

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

        // TODO
        constexpr float TonemapMin = 0.001f;
        constexpr float TonemapMax = 100.0f;
        for (uint32 phiInd = 0; phiInd < base.PhiCount; ++phiInd) {
            const float value     = comp->matrix()(row, col) / scale;
            const Vector4f mapped = colormap::plasma((value - TonemapMin) / (TonemapMax - TonemapMin));
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
    drawList.AddCircleFilled(transform(viewTheta, viewPhi), 8, ImColor(0, 100, 200));
}

void KlemsDataModel::renderProperties(float viewTheta, float viewPhi, DataComponent component)
{
    KlemsComponent* comp = getComponent(mKlems, component);
    if (!comp)
        return;

    // Get row of view vector
    int row = 0;
    for (const auto& base : comp->row()->thetaBasis()) {
        if (viewTheta < base.UpperTheta) {
            // Find phi
            row += std::min(base.PhiCount - 1, (uint32)std::floor(base.PhiCount * viewPhi / (2 * Pi)));
            break;
        } else {
            row += base.PhiCount;
        }
    }

    const float total   = comp->computeTotal();
    const float maxHemi = comp->computeMaxHemisphericalScattering();
    const float minProj = comp->computeMinimumProjectedSolidAngle();

    ImGui::Text("Incident Patch: %i", row);

    ImGui::Text("Total:    %3.2f", total);
    ImGui::Text("Max Hemi: %3.2f", maxHemi);
    ImGui::Text("Min Proj: %3.2f", minProj);
}
} // namespace IG