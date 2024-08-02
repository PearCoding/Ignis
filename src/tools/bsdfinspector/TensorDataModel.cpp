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

static inline Vector2f squareToConcentricDiskAngles(Vector2f p)
{
    p = 2 * p - Vector2f::Ones();

    if (p.squaredNorm() <= FltEps) {
        return Vector2f::Zero();
    } else if (p.x() * p.x() > p.y() * p.y()) {
        // Uses squares instead of absolute values
        // Top half
        float phi = Pi4 * p.y() / p.x();
        if (!std::isfinite(phi))
            phi = 0;
        if (p.x() < 0)
            phi -= Pi;
        if (phi < 0)
            phi += 2 * Pi;
        return Vector2f(std::abs(p.x()), phi);
    } else {
        // Bottom half
        float phi = Pi2 - Pi4 * p.x() / p.y();
        if (!std::isfinite(phi))
            phi = Pi2;
        if (p.y() < 0)
            phi -= Pi;
        if (phi < 0)
            phi += 2 * Pi;
        return Vector2f(std::abs(p.y()), phi);
    }
}

static inline Vector2f concentricDiskToSquare(const Vector2f& p)
{
    // const float phi = std::atan2(p.y(), p.x());
    // float r         = p.norm();

    // float a, b;
    // if (std::abs(phi) < Pi4 || std::abs(phi) > 3 * Pi4) {
    //     a = r = std::copysign(r, p.x());
    //     if (p.x() < 0) {
    //         if (p.y() < 0) {
    //             b = (Pi + phi) * r / Pi4;
    //         } else {
    //             b = (phi - Pi) * r / Pi4;
    //         }
    //     } else {
    //         b = (phi * r) / Pi4;
    //     }
    // } else {
    //     b = r = std::copysign(r, p.y());
    //     if (p.y() < 0) {
    //         a = -(Pi2 + phi) * r / Pi4;
    //     } else {
    //         a = (Pi2 - phi) * r / Pi4;
    //     }
    // }
    const bool quadrant = std::fabs(p.x()) > std::fabs(p.y());
    const float r_sign  = quadrant ? p.x() : p.y(); // If quadrant 0 or 2
    const float r       = std::copysign(p.squaredNorm(), r_sign);

    const float phi = std::atan2(std::copysign(p.y(), p.y() * r_sign), std::copysign(p.x(), p.x() * r_sign));

    const float c = 4 * phi / Pi;
    const float t = (quadrant ? c : 2 - c) * r;

    const float a = quadrant ? r : t;
    const float b = quadrant ? t : r;

    return Vector2f((a + 1) / 2, (b + 1) / 2);
}

static inline Vector2f composePoint(float incidentTheta, float incidentPhi, bool isotropic)
{
    const float r = incidentTheta / Pi2;

    if (isotropic) {
        const float proj = (0.5f - FltEps) - 0.5f * r; //???
        return concentricDiskToSquare(Vector2f(proj, 0));
    } else {
        const float x = r * std::cos(incidentPhi);
        const float y = r * std::sin(incidentPhi);
        return concentricDiskToSquare(Vector2f(x, y));
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
        pos *= 2;
        uint32 mask = 0;
        for (int d = 0; d < 2; ++d) {
            if (pos[d] >= 1) {
                mask |= (1 << d);
                pos[d] -= 1;
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

    const Vector2f minRPhi = squareToConcentricDiskAngles(minPos);
    const Vector2f maxRPhi = squareToConcentricDiskAngles(maxPos);

    const float theta0 = minRPhi.x() * Pi2;
    const float phi0   = minRPhi.y();
    const float theta1 = maxRPhi.x() * Pi2;
    const float phi1   = maxRPhi.y();

    drawList.AddQuadFilled(transform(theta0, phi0), transform(theta0, phi1), transform(theta1, phi1), transform(theta1, phi0), ImColor(color.x(), color.y(), color.z()));
    drawList.AddQuad(transform(theta0, phi0), transform(theta0, phi1), transform(theta1, phi1), transform(theta1, phi0), LineColor);
}

template <typename TransformF>
static void drawPatchRecursive(const TensorTreeNode* node, const Vector2f& incidentPos, const Vector2f& minPos, const Vector2f& maxPos, TransformF transform, const Colormapper& mapper)
{
    const auto subset     = queryChildSubset(node, incidentPos);
    const Vector2f midPos = (minPos + maxPos) / 2;
    if (node->isLeaf()) {
        // No idea why the order is reversed in this case (see tt_lookup_leaf in tensortree.art, which is given by bsdf_t.c in Radiance)
        drawPatch(minPos, midPos, mapper.apply(node->Values[3]), transform);
        drawPatch(Vector2f(midPos.x(), minPos.y()), Vector2f(maxPos.x(), midPos.y()), mapper.apply(node->Values[2]), transform);
        drawPatch(Vector2f(minPos.x(), midPos.y()), Vector2f(midPos.x(), maxPos.y()), mapper.apply(node->Values[1]), transform);
        drawPatch(midPos, maxPos, mapper.apply(node->Values[0]), transform);
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