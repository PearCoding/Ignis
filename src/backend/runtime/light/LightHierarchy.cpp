#include "LightHierarchy.h"
#include "Light.h"
#include "container/PointBvh.h"
#include "loader/LoaderContext.h"
#include "loader/ShadingTree.h"
#include "serialization/FileSerializer.h"

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
        // 16 floats
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

LightEntry populateInnerNodes(size_t id, const LightBvh& bvh, std::vector<LightEntry>& entries)
{
    const auto& node = bvh.innerNodes()[id];
    auto& entry      = entries[id];
    if (node.isLeaf()) {
        const auto leaf = bvh.leafNodes().at(node.Index);
        entry           = leaf;
    } else {
        const auto left  = populateInnerNodes(node.leftIndex(), bvh, entries);
        const auto right = populateInnerNodes(node.rightIndex(), bvh, entries);

        entry.Position  = node.BBox.center();
        entry.Direction = (left.Direction + right.Direction) / 2;
        entry.Flux      = left.Flux + right.Flux;
        entry.ID        = -int32(node.leftIndex() + 1);
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
        const Vector3f dir = l->direction().value_or(Vector3f::UnitZ());
        const float flux   = l->computeFlux(tree);

        bvh.store(LightEntry(p.value(), dir, flux, (int32)l->id()));
    }
 
    // Compute flux and average direction for inner nodes
    std::vector<LightEntry> innerNodes(bvh.innerNodes().size());
    populateInnerNodes(0, bvh, innerNodes);

    // Export bvh
    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/light_hierarchy.bin";

    FileSerializer serializer(path, false);
    serializer.write(innerNodes, true);

    tree.context().ExportedData[exported_id] = path;
    return path;
}
} // namespace IG