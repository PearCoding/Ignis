#include "TensorDataModel.h"
#include "Colormapper.h"
#include "measured/TensorTreeLoader.h"

#include "UI.h"

namespace IG {
TensorDataModel::TensorDataModel(const std::shared_ptr<TensorTree>& ttree)
    : mTensorTree(ttree)
{
}

TensorDataModel::~TensorDataModel() {}

static inline Vector2f composePoint(float incidentTheta, float incidentPhi, bool isotropic)
{
    incidentTheta /= Pi2;
    if (incidentPhi < 0)
        incidentPhi += 2 * Pi;
    incidentPhi = std::fmod(incidentPhi, 2 * Pi) / 2 * Pi;

    if (isotropic) {
        const float proj = (0.5f - FltEps) - 0.5f * incidentTheta; //???
        return Vector2f(proj, 0);
    } else {
        return Vector2f(incidentTheta, incidentPhi);
    }
}

static inline TensorTreeComponent* getComponent(const std::shared_ptr<TensorTree>& ttree, DataComponent component)
{
    switch (component) {
    default:
    case DataComponent::FrontReflection:
        return ttree->front_reflection.get();
    case DataComponent::FrontTransmission:
        return ttree->front_transmission.get();
    case DataComponent::BackReflection:
        return ttree->back_reflection.get();
    case DataComponent::BackTransmission:
        return ttree->back_transmission.get();
    }
}

struct QueryPos {
    const TensorTreeNode* Child;
    Vector4f NormalizedPos;
    Vector4f MinPos;
    Vector4f MaxPos;
};

[[nodiscard]] static inline QueryPos queryChild(const TensorTreeNode* node, Vector4f pos, Vector4f minPos, Vector4f maxPos)
{
    if (node->isLeaf())
        return QueryPos{ .Child = nullptr, .NormalizedPos = pos, .MinPos = minPos, .MaxPos = maxPos };

    const bool isIsotropic = node->Children.size() == 4; // Why 4 and not 8??
    if (isIsotropic) {
        // TODO
        return QueryPos{ .Child = nullptr, .NormalizedPos = pos, .MinPos = minPos, .MaxPos = maxPos };
    } else {
        int mask = 0;
        for (int d = 0; d < 4; ++d) {
            if (pos[d] >= 0.5f) {
                mask |= (1 << d);
                pos[d]    = (pos[d] - 0.5f) * 2;
                minPos[d] = (minPos[d] + maxPos[d]) / 2;
            } else {
                pos[d]    = pos[d] * 2;
                maxPos[d] = (minPos[d] + maxPos[d]) / 2;
            }
        }

        return QueryPos{ .Child = node->Children[mask].get(), .NormalizedPos = pos, .MinPos = minPos, .MaxPos = maxPos };
    }
}

struct QuerySubset {
    uint32 ChildOffset;
    Vector2f NormalizedPos;
    uint32 Pitch;
};

[[nodiscard]] static inline QuerySubset queryChildSubset(const TensorTreeNode* node, Vector2f pos)
{
    const bool isIsotropic = node->Children.size() == 4; // Why 4 and not 8??
    if (isIsotropic) {
        // TODO
        return QuerySubset{ .ChildOffset = 0, .NormalizedPos = pos, .Pitch = 1 };
    } else {
        uint32 mask = 0;
        for (int d = 0; d < 2; ++d) {
            if (pos[d] >= 0.5f) {
                mask |= (1 << d);
                pos[d] = (pos[d] - 0.5f) * 2;
            } else {
                pos[d] = pos[d] * 2;
            }
        }

        return QuerySubset{ .ChildOffset = mask, .NormalizedPos = pos, .Pitch = 4 };
    }
}

static const ImColor LineColor = ImColor(200, 200, 200);

template <typename TransformF>
static inline void drawPatch(const Vector2f& minPos, const Vector2f& maxPos, const Vector4f& color, TransformF transform)
{
    auto& drawList = *ImGui::GetWindowDrawList();

    const float theta0 = minPos.x() * Pi2;
    const float phi0   = minPos.y() * 2 * Pi;
    const float theta1 = maxPos.x() * Pi2;
    const float phi1   = maxPos.y() * 2 * Pi;

    const int patches = std::max(1, (int)std::floor(64 * (phi1 - phi0) / Pi));

    for (int i = 0; i < patches; ++i) {
        const float t0 = i / (float)patches;
        const float t1 = (i + 1) / (float)patches;
        const float p0 = phi0 * (1 - t0) + phi1 * t0;
        const float p1 = phi0 * (1 - t1) + phi1 * t1;

        drawList.AddQuadFilled(transform(theta0, p0), transform(theta0, p1), transform(theta1, p1), transform(theta1, p0), ImColor(color.x(), color.y(), color.z()));
        drawList.AddLine(transform(theta0, p0), transform(theta0, p1), LineColor);
        drawList.AddLine(transform(theta1, p0), transform(theta1, p1), LineColor);
    }

    drawList.AddLine(transform(theta0, phi0), transform(theta1, phi0), LineColor);
    drawList.AddLine(transform(theta0, phi1), transform(theta1, phi1), LineColor);
}

template <typename TransformF>
static void drawPatchRecursive(const TensorTreeNode* node, const Vector2f& incidentPos, const Vector2f& minPos, const Vector2f& maxPos, TransformF transform, const Colormapper& mapper)
{
    const auto subset     = queryChildSubset(node, incidentPos);
    const Vector2f midPos = (minPos + maxPos) / 2;
    if (node->isLeaf()) {
        drawPatch(minPos, midPos, mapper.apply(node->Values[0 * subset.Pitch]), transform);
        drawPatch(Vector2f(midPos.x(), minPos.y()), Vector2f(maxPos.x(), midPos.y()), mapper.apply(node->Values[1 * subset.Pitch]), transform);
        drawPatch(Vector2f(minPos.x(), midPos.y()), Vector2f(midPos.x(), maxPos.y()), mapper.apply(node->Values[2 * subset.Pitch]), transform);
        drawPatch(midPos, maxPos, mapper.apply(node->Values[3 * subset.Pitch]), transform);
    } else {
        drawPatchRecursive(node->Children[0 * subset.Pitch].get(), subset.NormalizedPos, minPos, midPos, transform, mapper);
        drawPatchRecursive(node->Children[1 * subset.Pitch].get(), subset.NormalizedPos, Vector2f(midPos.x(), minPos.y()), Vector2f(maxPos.x(), midPos.y()), transform, mapper);
        drawPatchRecursive(node->Children[2 * subset.Pitch].get(), subset.NormalizedPos, Vector2f(minPos.x(), midPos.y()), Vector2f(midPos.x(), maxPos.y()), transform, mapper);
        drawPatchRecursive(node->Children[3 * subset.Pitch].get(), subset.NormalizedPos, midPos, maxPos, transform, mapper);
    }
}

void TensorDataModel::renderView(float incidentTheta, float incidentPhi, float radius, DataComponent component, const Colormapper& mapper)
{
    TensorTreeComponent* comp = getComponent(mTensorTree, component);
    if (!comp)
        return;

    const ImVec2 area   = ImGui::GetContentRegionAvail();
    const ImVec2 center = ImVec2(ImGui::GetCursorScreenPos().x + area.x / 2, ImGui::GetCursorScreenPos().y + area.y / 2);

    const auto radiusFromTheta = [&](float theta) {
        return radius * theta / Pi2;
    };

    const auto transform = [&](float theta, float phi) {
        const float r = radiusFromTheta(theta);
        const float x = std::cos(phi);
        const float y = std::sin(phi);
        return ImVec2(r * x + center.x, r * y + center.y);
    };
    auto& drawList = *ImGui::GetWindowDrawList();

    // Draw patches
    if (comp->isRootLeaf()) {
        IG_ASSERT(comp->root()->isLeaf(), "Invalid root configuration");
        const Vector4f color = mapper.apply(comp->root()->Values[0]);
        drawList.AddCircleFilled(center, radius, ImColor(color.x(), color.y(), color.z()));
    } else {
        drawPatchRecursive(comp->root(), composePoint(incidentTheta, incidentPhi, mTensorTree->is_isotropic), Vector2f::Zero(), Vector2f::Ones(), transform, mapper);
    }

    // Draw encircling circle
    drawList.AddCircle(center, radius, LineColor);

    // Draw view point
    drawList.AddCircleFilled(transform(incidentTheta, incidentPhi), 8, ImColor(0, 100, 200));
}

void TensorDataModel::renderInfo(float incidentTheta, float incidentPhi, float outgoingTheta, float outgoingPhi, DataComponent component)
{
}

void TensorDataModel::renderProperties(float incidentTheta, float incidentPhi, DataComponent component)
{
    TensorTreeComponent* comp = getComponent(mTensorTree, component);
    if (!comp)
        return;

    ImGui::Text("Is Isotropic: %s", mTensorTree->is_isotropic ? "Yes" : "No");
    ImGui::Text("Total:        %3.2f", comp->total());
    ImGui::Text("Min Proj:     %3.2f", comp->minProjSA());
    ImGui::Text("Max Depth:    %zu", comp->maxDepth());
    ImGui::Text("Node Count:   %zu", comp->nodeCount());
    ImGui::Text("Value Count:  %zu", comp->valueCount());
}
} // namespace IG