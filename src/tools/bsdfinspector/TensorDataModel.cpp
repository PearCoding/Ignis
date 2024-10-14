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
        return Vector2f(std::abs(p.x()) * Pi2, phi);
    } else {
        // Bottom half
        float phi = Pi2 - Pi4 * p.x() / p.y();
        if (!std::isfinite(phi))
            phi = Pi2;
        if (p.y() < 0)
            phi -= Pi;
        if (phi < 0)
            phi += 2 * Pi;
        return Vector2f(std::abs(p.y()) * Pi2, phi);
    }
}

static inline Vector2f concentricDiskToSquare(const Vector2f& p)
{
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

static inline Vector2f composePoint(float theta, float phi, bool isotropic)
{
    const float r = theta / Pi2;

    if (isotropic) {
        const float proj = (0.5f - FltEps) - 0.5f * r; //TODO: Missing rotation of the outgoing direction based on the theta
        return concentricDiskToSquare(Vector2f(proj, 0));
    } else {
        const float x = r * std::cos(phi);
        const float y = r * std::sin(phi);
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
    uint32 Index;
    Vector4f NormalizedPos;
    Vector4f MinPos;
    Vector4f MaxPos;
};

[[nodiscard]] static inline QueryPos queryChild(const TensorTreeNode* node, Vector4f pos, Vector4f minPos, Vector4f maxPos, bool isotropic)
{
    const int incidentDim = isotropic ? 1 : 2;
    const bool isLeaf     = node->isLeaf();
    const Vector4f midPos = (minPos + maxPos) / 2;

    pos *= 2;

    uint32 mask = 0;
    uint32 c    = 0;
    for (int d = 0; d < 4; ++d) {
        if (d == 1 && isotropic)
            continue;
        if (pos[d] >= 1) {
            mask |= (1 << (isLeaf ? (2 + incidentDim - c + -1) : c));
            pos[d] -= 1;
            minPos[d] = midPos[d];
        } else {
            maxPos[d] = midPos[d];
        }
        ++c;
    }

    return QueryPos{ .Child = isLeaf ? nullptr : node->Children[mask].get(), .Index = mask, .NormalizedPos = pos, .MinPos = minPos, .MaxPos = maxPos };
}

struct QuerySubset {
    uint32 Mask;
    Vector2f NormalizedPos;
};

[[nodiscard]] static inline QuerySubset queryChildSubset(const TensorTreeNode* node, Vector2f incidentPos, bool isotropic)
{
    const int incidentDim = isotropic ? 1 : 2;
    const bool isLeaf     = node->isLeaf();

    incidentPos *= 2;

    uint32 mask = 0;
    for (int d = 0; d < incidentDim; ++d) {
        if (incidentPos[d] >= 1) {
            mask |= (1 << (isLeaf ? (2 + incidentDim - d + -1) : d));
            incidentPos[d] -= 1;
        }
    }

    return QuerySubset{ .Mask = mask, .NormalizedPos = incidentPos };
}

template <typename TransformF>
static inline void drawLineSegment(ImDrawList& drawList, const Vector2f& minPos, const Vector2f& maxPos, TransformF transform, int segments)
{
    const Vector2f delta = (maxPos - minPos) / segments;
    for (int i = 0; i < segments; ++i) {
        drawList.PathLineTo(transform(minPos + (i + 1) * delta));
    }
}

static const ImColor LineColor = ImColor(200, 200, 200);
template <typename TransformF>
static inline void drawPatch(const Vector4f& color, const Vector2f& minPos, const Vector2f& maxPos, TransformF transform, int depth)
{
    auto& drawList = *ImGui::GetWindowDrawList();

    const int segments = std::max(1, 32 / (1 << depth));

    const auto& transform2 = [&](const Vector2f& p) {
        const Vector2f rPhi = squareToConcentricDiskAngles(p);
        const float theta   = rPhi.x();
        const float phi     = rPhi.y() + Pi; // Rotate 180° for outgoing
        return transform(theta, phi);
    };

    drawList.PathClear();
    drawLineSegment(drawList, minPos, Vector2f(minPos.x(), maxPos.y()), transform2, segments);
    drawLineSegment(drawList, Vector2f(minPos.x(), maxPos.y()), maxPos, transform2, segments);
    drawLineSegment(drawList, maxPos, Vector2f(maxPos.x(), minPos.y()), transform2, segments);
    drawLineSegment(drawList, Vector2f(maxPos.x(), minPos.y()), minPos, transform2, segments);
    drawList.AddConvexPolyFilled(drawList._Path.Data, drawList._Path.Size, ImColor(color.x(), color.y(), color.z()));
    drawList.PathStroke(LineColor, ImDrawFlags_Closed, 1.0f);
}

template <typename TransformF>
static void drawPatchRecursive(const TensorTreeNode* node, const Vector2f& incidentPos, const Vector2f& minPos, const Vector2f& maxPos, TransformF transform, const Colormapper& mapper, int depth, bool isotropic)
{
    const uint32 incidentDim = isotropic ? 1 : 2;
    const auto subset        = queryChildSubset(node, incidentPos, isotropic);
    const Vector2f midPos    = (minPos + maxPos) / 2;
    if (node->isLeaf()) {
        if (node->Values.size() == 1) {
            drawPatch(mapper.apply(node->Values.at(0)), minPos, maxPos, transform, depth);
        } else {
            IG_ASSERT(node->Values.size() == 4 || node->Values.size() == 16, "Expected valid value size");
            // No idea why the order is reversed in this case (see tt_lookup_leaf in tensortree.art, which is given by bsdf_t.c in Radiance)
            drawPatch(mapper.apply(node->Values.at(subset.Mask | 0x0)), minPos, midPos, transform, depth + 1);                                                     // 00
            drawPatch(mapper.apply(node->Values.at(subset.Mask | 0x2)), Vector2f(midPos.x(), minPos.y()), Vector2f(maxPos.x(), midPos.y()), transform, depth + 1); // 01
            drawPatch(mapper.apply(node->Values.at(subset.Mask | 0x1)), Vector2f(minPos.x(), midPos.y()), Vector2f(midPos.x(), maxPos.y()), transform, depth + 1); // 10
            drawPatch(mapper.apply(node->Values.at(subset.Mask | 0x3)), midPos, maxPos, transform, depth + 1);                                                     // 11
        }
    } else {
        drawPatchRecursive(node->Children.at(subset.Mask | (0x0 << incidentDim)).get(), subset.NormalizedPos, minPos, midPos, transform, mapper, depth + 1, isotropic);                                                     // 00
        drawPatchRecursive(node->Children.at(subset.Mask | (0x1 << incidentDim)).get(), subset.NormalizedPos, Vector2f(midPos.x(), minPos.y()), Vector2f(maxPos.x(), midPos.y()), transform, mapper, depth + 1, isotropic); // 10
        drawPatchRecursive(node->Children.at(subset.Mask | (0x2 << incidentDim)).get(), subset.NormalizedPos, Vector2f(minPos.x(), midPos.y()), Vector2f(midPos.x(), maxPos.y()), transform, mapper, depth + 1, isotropic); // 01
        drawPatchRecursive(node->Children.at(subset.Mask | (0x3 << incidentDim)).get(), subset.NormalizedPos, midPos, maxPos, transform, mapper, depth + 1, isotropic);                                                     // 11
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
    if (comp->root()) {
        if (comp->isRootLeaf()) {
            IG_ASSERT(comp->root()->isLeaf(), "Invalid root configuration");
            const Vector4f color = mapper.apply(comp->root()->Values.at(0));
            drawList.AddCircleFilled(center, radius, ImColor(color.x(), color.y(), color.z()));
        } else {
            drawPatchRecursive(comp->root(), composePoint(incidentTheta, incidentPhi, mTensorTree->is_isotropic), Vector2f::Zero(), Vector2f::Ones(), transform, mapper, 0, mTensorTree->is_isotropic);
        }
    }

    // Draw encircling circle
    drawList.AddCircle(center, radius, LineColor);

    // Draw view point
    drawList.AddCircleFilled(transform(incidentTheta, incidentPhi), 8, ImColor(0, 100, 200));
}

static inline Vector4f map4StoCDA(const Vector4f& pos)
{
    const Vector2f a = squareToConcentricDiskAngles(pos.block<2, 1>(0, 0));
    const Vector2f b = squareToConcentricDiskAngles(pos.block<2, 1>(2, 0));
    return Vector4f(a.x(), a.y(), b.x(), b.y());
}

void TensorDataModel::renderInfo(float incidentTheta, float incidentPhi, float outgoingTheta, float outgoingPhi, DataComponent component)
{
    TensorTreeComponent* comp = getComponent(mTensorTree, component);
    if (!comp)
        return;

    if (!ImGui::BeginTooltip())
        return;

    const Vector2f incidentPos  = composePoint(incidentTheta, incidentPhi, mTensorTree->is_isotropic);
    const Vector2f outgoingPos  = composePoint(outgoingTheta, outgoingPhi + Pi, false);
    const Vector4f globalPos    = Vector4f(incidentPos.x(), incidentPos.y(), outgoingPos.x(), outgoingPos.y());
    const Vector4f globalAngles = map4StoCDA(globalPos);

    const TensorTreeNode* node = comp->root();
    Vector4f currentPos        = globalPos;
    Vector4f minPos            = Vector4f::Zero();
    Vector4f maxPos            = Vector4f::Ones();
    while (node && !node->isLeaf()) {
        const auto query = queryChild(node, currentPos, minPos, maxPos, mTensorTree->is_isotropic);

        node       = query.Child;
        currentPos = query.NormalizedPos;
        minPos     = query.MinPos;
        maxPos     = query.MaxPos;
    }

    if (!node) {
        ImGui::Text("Invalid query");
    } else {
        IG_ASSERT(node->isLeaf(), "Expected leaf");
        const auto query = queryChild(node, currentPos, minPos, maxPos, mTensorTree->is_isotropic);

        IG_ASSERT(node->Values.size() == 1 || node->Values.size() == 4 || node->Values.size() == 16, "Expected valid value size");
        const float value = node->Values.size() == 1 ? node->Values.at(0) : node->Values.at(query.Index);

        const Vector4f minAngles = map4StoCDA(query.MinPos);
        const Vector4f maxAngles = map4StoCDA(query.MaxPos);

        ImGui::Text("Value:  %3.2f", value);
        ImGui::Separator();
        ImGui::Text("Global: [%3.2f°, %3.2f°, %3.2f°, %3.2f°]", globalAngles.x() * Rad2Deg, globalAngles.y() * Rad2Deg, globalAngles.z() * Rad2Deg, globalAngles.w() * Rad2Deg);
        ImGui::Text("Min:    [%3.2f°, %3.2f°, %3.2f°, %3.2f°]", minAngles.x() * Rad2Deg, minAngles.y() * Rad2Deg, minAngles.z() * Rad2Deg, minAngles.w() * Rad2Deg);
        ImGui::Text("Max:    [%3.2f°, %3.2f°, %3.2f°, %3.2f°]", maxAngles.x() * Rad2Deg, maxAngles.y() * Rad2Deg, maxAngles.z() * Rad2Deg, maxAngles.w() * Rad2Deg);
        ImGui::Separator();
        ImGui::Text("Global: [%3.2f,  %3.2f,  %3.2f,  %3.2f]", globalPos.x(), globalPos.y(), globalPos.z(), globalPos.w());
        ImGui::Text("Min:    [%3.2f,  %3.2f,  %3.2f,  %3.2f]", query.MinPos.x(), query.MinPos.y(), query.MinPos.z(), query.MinPos.w());
        ImGui::Text("Max:    [%3.2f,  %3.2f,  %3.2f,  %3.2f]", query.MaxPos.x(), query.MaxPos.y(), query.MaxPos.z(), query.MaxPos.w());
    }
    ImGui::EndTooltip();
}

void TensorDataModel::renderProperties(float incidentTheta, float incidentPhi, DataComponent component)
{
    TensorTreeComponent* comp = getComponent(mTensorTree, component);
    if (!comp)
        return;

    const Vector2f incidentPos    = composePoint(incidentTheta, incidentPhi, mTensorTree->is_isotropic);
    const Vector2f incidentAngles = squareToConcentricDiskAngles(incidentPos);

    ImGui::Text("Is Isotropic: %s", mTensorTree->is_isotropic ? "Yes" : "No");
    ImGui::Text("Total:        %3.2f", comp->total());
    ImGui::Text("Min Proj:     %3.6f", comp->minProjSA());
    ImGui::Text("Max Depth:    %zu", comp->maxDepth());
    ImGui::Text("Node Count:   %zu", comp->nodeCount());
    ImGui::Text("Value Count:  %zu", comp->valueCount());
    ImGui::Text("Incident Pos: [%3.2f°, %3.2f°] | [%3.2f, %3.2f]", incidentAngles.x() * Rad2Deg, incidentAngles.y() * Rad2Deg, incidentPos.x(), incidentPos.y());
}
} // namespace IG