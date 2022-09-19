#include "LightHierarchy.h"
#include "Light.h"
#include "container/PointBvh.h"
#include "loader/LoaderContext.h"
#include "loader/ShadingTree.h"
#include "serialization/FileSerializer.h"

#include "Logger.h"

namespace IG {
class LightEntry : public ISerializable {
public:
    Vector3f Position;
    Vector3f Direction;
    float Flux;
    int32 ID;

    inline LightEntry() = default;
    inline LightEntry(const Vector3f& pos, const Vector3f& dir, float flux, int32 id)
        : ISerializable()
        , Position(pos)
        , Direction(dir)
        , Flux(flux)
        , ID(id)
    {
    }

    inline void serialize(Serializer& serializer) override
    {
        serializer.write(Position);
        serializer.write(Flux);
        serializer.write(Direction);
        serializer.write(ID);
        // 8 floats
    }
};

template <>
struct DefaultPositionGetter<LightEntry> {
    inline Vector3f operator()(const LightEntry& p) const
    {
        return p.Position;
    }
};

using LightBvh = PointBvh<LightEntry>;

inline static LightEntry populateInnerNodes(size_t id, uint32 code, uint32 depth, const LightBvh& bvh, std::vector<LightEntry>& entries, std::vector<uint32>& codes)
{
    const auto& node = bvh.innerNodes().at(id);
    auto& entry      = entries[id];
    if (node.isLeaf()) {
        const auto leaf = bvh.leafNodes().at(node.Index);
        entry           = leaf;
        codes[leaf.ID]  = code;
    } else {
        const auto left  = populateInnerNodes(node.leftIndex(), code, depth + 1, bvh, entries, codes);
        const auto right = populateInnerNodes(node.rightIndex(), code | (0x1 << depth), depth + 1, bvh, entries, codes);

        entry.Position = node.BBox.center();
        entry.ID       = -int32(node.leftIndex() + 1);

        // Handle delta cases
        if (left.Flux < 0 && right.Flux < 0) {
            entry.Direction = Vector3f::UnitZ();
            entry.Flux      = left.Flux + right.Flux;
        } else if (left.Flux < 0) {
            entry.Direction = Vector3f::UnitZ();
            entry.Flux      = -(-left.Flux + right.Flux);
        } else if (right.Flux < 0) {
            entry.Direction = Vector3f::UnitZ();
            entry.Flux      = -(left.Flux - right.Flux);
        } else {
            entry.Direction = (left.Direction + right.Direction).normalized();
            entry.Flux      = left.Flux + right.Flux;
        }
    }
    return entry;
}

std::filesystem::path LightHierarchy::setup(const std::vector<std::shared_ptr<Light>>& lights, ShadingTree& tree)
{
    if (lights.empty())
        return {};

    // Check if already setup
    const std::string exported_id = "_light_hierarchy_";

    const auto data = tree.context().ExportedData.find(exported_id);
    if (data != tree.context().ExportedData.end())
        return std::any_cast<std::string>(data->second);

    // Setup bvh
    LightBvh bvh;
    for (const auto& l : lights) {
        const auto p = l->position();
        IG_ASSERT(p.has_value(), "Expected all finite lights to return a valid position");
        const bool has_dir = l->direction().has_value();
        const Vector3f dir = l->direction().value_or(Vector3f::UnitZ());
        const float flux   = l->computeFlux(tree);

        bvh.store(LightEntry(p.value(), dir, has_dir ? flux : -flux, (int32)l->id()));
    }

    // Compute flux and average direction for inner nodes
    std::vector<LightEntry> innerNodes(bvh.innerNodes().size());
    std::vector<uint32> codes(lights.size(), 0);
    populateInnerNodes(0, 0, 0, bvh, innerNodes, codes);

    if (L_DEBUG == IG_LOGGER.verbosity()) {
        IG_LOG(L_DEBUG) << "Light Hierarchy:" << std::endl;
        for (const auto& node : innerNodes)
            IG_LOG(L_DEBUG) << (node.ID < 0 ? "Node" : "Leaf") << ": [" << node.Position.transpose() << ", " << node.Direction.transpose() << ", " << node.Flux << ", " << node.ID << "];" << std::endl;

        for (const auto& code : codes)
            IG_LOG(L_DEBUG) << code << std::endl;
    }

    // Export bvh
    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/light_hierarchy.bin";

    FileSerializer serializer(path, false);
    serializer.write(codes, true); // Codes are used to backtrack for the pdf. TODO: Currently this is limited to max depth 32!
    serializer.writeAlignmentPad(sizeof(float) * 4);
    serializer.write(innerNodes, true);

    tree.context().ExportedData[exported_id] = path;
    return path;
}
} // namespace IG