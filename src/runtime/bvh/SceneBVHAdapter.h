#pragma once

#include "BvhNAdapter.h"
#include "device/Target.h"
#include "math/BoundingBox.h"

#include <ranges>

IG_BEGIN_IGNORE_WARNINGS
#include <bvh/v2/default_builder.h>
IG_END_IGNORE_WARNINGS

// Contains implementation for NodeN
#include "generated_interface.h"

namespace IG {
struct EntityObject {
    BoundingBox BBox;
    int32 EntityID;
    int32 ShapeID;
    int32 MaterialID;
    int32 User1ID;
    int32 User2ID;
    Matrix4f Local;
    uint32 Flags;

    using ScalarType = float;
    [[nodiscard]] inline bvh::Bbox bounding_box() const { return bvh::from(BBox); }
    [[nodiscard]] inline bvh::Vec3 center() const { return bvh::from(BBox.center()); }
};

template <size_t N>
struct BvhNEnt {
};

template <>
struct BvhNEnt<8> {
    using Node = Node8;
};

template <>
struct BvhNEnt<4> {
    using Node = Node4;
};

template <>
struct BvhNEnt<2> {
    using Node = Node2;
};

template <size_t N, std::ranges::random_access_range PrimitiveRange>
class BvhNEntAdapter : public BvhNAdapter<N, typename BvhNEnt<N>::Node, PrimitiveRange> {
    using Parent = BvhNAdapter<N, typename BvhNEnt<N>::Node, PrimitiveRange>;
    using Bvh    = typename Parent::Bvh;
    using Node   = typename BvhNEnt<N>::Node;
    using Obj    = EntityLeaf1;

    std::vector<Obj>& objects;

public:
    BvhNEntAdapter(std::vector<Node>& nodes, const PrimitiveRange& primitives, std::vector<Obj>& objects)
        : Parent(nodes, primitives)
        , objects(objects)
    {
    }

protected:
    virtual void write_leaf(const Bvh& bvh,
                            const typename Bvh::Node& node,
                            size_t parent,
                            size_t child)
        override
    {
        IG_ASSERT(node.is_leaf(), "Expected a leaf");

        this->nodes[parent].child.e[child] = ~static_cast<int>(objects.size());

        for (size_t i = 0; i < this->primitive_count_of_node(node); ++i) {
            const int id      = (int)bvh.primitive_indices.at(node.first_child_or_primitive + i);
            const auto in_obj = std::ranges::cbegin(this->primitives)[id];
            // IG_ASSERT(in_obj.EntityID == id, "Expected entity order in BVH match entity id!");

            objects.emplace_back(EntityLeaf1{
                { in_obj.BBox.min(0), in_obj.BBox.min(1), in_obj.BBox.min(2) },
                in_obj.EntityID,
                { in_obj.BBox.max(0), in_obj.BBox.max(1), in_obj.BBox.max(2) },
                in_obj.ShapeID,
                { { { { in_obj.Local(0, 0), in_obj.Local(1, 0), in_obj.Local(2, 0) },
                      { in_obj.Local(0, 1), in_obj.Local(1, 1), in_obj.Local(2, 1) },
                      { in_obj.Local(0, 2), in_obj.Local(1, 2), in_obj.Local(2, 2) },
                      { in_obj.Local(0, 3), in_obj.Local(1, 3), in_obj.Local(2, 3) } } } },
                in_obj.Flags,
                in_obj.MaterialID,
                in_obj.User1ID,
                in_obj.User2ID });
        }

        // Add sentinel
        objects.back().entity_id |= 0x80000000;
    }
};

template <size_t N, std::ranges::random_access_range PrimitiveRange>
static inline BvhNEntAdapter<N, PrimitiveRange> make_bvh_adapter(std::vector<typename BvhNEnt<N>::Node>& nodes, const PrimitiveRange& primitives, std::vector<EntityLeaf1>& objs)
{
    return BvhNEntAdapter<N, PrimitiveRange>(nodes, primitives, objs);
}

template <size_t N>
inline void build_scene_bvh(std::vector<typename BvhNEnt<N>::Node>& nodes,
                            std::vector<EntityLeaf1>& objs,
                            std::vector<EntityObject> primitives)
{
    const auto bboxes_r  = (primitives | std::views::transform([](const EntityObject& o) -> const bvh::Bbox { return o.bounding_box(); }));
    const auto centers_r = (primitives | std::views::transform([](const EntityObject& o) -> const bvh::Vec3 { return o.center(); }));

    // FIXME: libbvh expects a span which is a continuous view, but range returns a random-access-view
    // Might require changes in libbvh
    const std::vector<bvh::Bbox> bboxes(bboxes_r.begin(), bboxes_r.end());
    const std::vector<bvh::Vec3> centers(centers_r.begin(), centers_r.end());

    using Builder = ::bvh::v2::DefaultBuilder<bvh::Node>;
    typename Builder::Config config;
    config.quality       = Builder::Quality::High;
    config.max_leaf_size = 1;
    auto bvh             = Builder::build(bboxes, centers);

    make_bvh_adapter<N>(nodes, primitives, objs).adapt(bvh);
}
} // namespace IG