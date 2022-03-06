#pragma once

#include "BvhNAdapter.h"
#include "Target.h"
#include "math/BoundingBox.h"

IG_BEGIN_IGNORE_WARNINGS
#include <bvh/bvh.hpp>
#include <bvh/leaf_collapser.hpp>
#include <bvh/locally_ordered_clustering_builder.hpp>
#include <bvh/node_layout_optimizer.hpp>
#include <bvh/parallel_reinsertion_optimizer.hpp>
#include <bvh/spatial_split_bvh_builder.hpp>
#include <bvh/sweep_sah_builder.hpp>
#include <bvh/triangle.hpp>
IG_END_IGNORE_WARNINGS

// Contains implementation for NodeN
#include "generated_interface.h"

namespace IG {
struct EntityObject {
    BoundingBox BBox;
    uint32 ShapeID;
    Matrix4f Local;

    using ScalarType = float;
    inline bvh::BoundingBox<float> bounding_box() const { return bvh::BoundingBox<float>(BBox.min, BBox.max); }
    inline bvh::Vector3<float> center() const { return BBox.center(); }
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

template <size_t N, template <typename> typename Allocator>
class BvhNEntAdapter : public BvhNAdapter<N, typename BvhNEnt<N>::Node, EntityObject, Allocator> {
    using Parent = BvhNAdapter<N, typename BvhNEnt<N>::Node, EntityObject, Allocator>;
    using Bvh    = typename Parent::Bvh;
    using Node   = typename BvhNEnt<N>::Node;
    using Obj    = EntityLeaf1;

    std::vector<Obj, Allocator<Obj>>& objects;

public:
    BvhNEntAdapter(std::vector<Node, Allocator<Node>>& nodes, std::vector<Obj, Allocator<Obj>>& objects)
        : Parent(nodes)
        , objects(objects)
    {
    }

protected:
    virtual void write_leaf(const std::vector<EntityObject>& primitives,
                            const Bvh& bvh,
                            const typename Bvh::Node& node,
                            size_t parent,
                            size_t child)
        override
    {
        IG_ASSERT(node.is_leaf(), "Expected a leaf");

        this->nodes[parent].child.e[child] = ~static_cast<int>(objects.size());

        for (size_t i = 0; i < this->primitive_count_of_node(node); ++i) {
            const int id       = (int)bvh.primitive_indices[node.first_child_or_primitive + i];
            const auto& in_obj = primitives[id];

            objects.emplace_back(EntityLeaf1{
                { in_obj.BBox.min(0), in_obj.BBox.min(1), in_obj.BBox.min(2) },
                id,
                { in_obj.BBox.max(0), in_obj.BBox.max(1), in_obj.BBox.max(2) },
                (int)in_obj.ShapeID,
                { { { { in_obj.Local(0, 0), in_obj.Local(1, 0), in_obj.Local(2, 0) },
                      { in_obj.Local(0, 1), in_obj.Local(1, 1), in_obj.Local(2, 1) },
                      { in_obj.Local(0, 2), in_obj.Local(1, 2), in_obj.Local(2, 2) },
                      { in_obj.Local(0, 3), in_obj.Local(1, 3), in_obj.Local(2, 3) } } } } });
        }

        // Add sentinel
        objects.back().entity_id |= 0x80000000;
    }
};

template <size_t N, template <typename> typename Allocator>
inline void build_scene_bvh(std::vector<typename BvhNEnt<N>::Node, Allocator<typename BvhNEnt<N>::Node>>& nodes,
                            std::vector<EntityLeaf1, Allocator<EntityLeaf1>>& objs,
                            std::vector<EntityObject, Allocator<EntityObject>>& primitives)
{
    using Bvh = bvh::Bvh<float>;
    // using BvhBuilder = bvh::SweepSahBuilder<Bvh>;
    using BvhBuilder = bvh::LocallyOrderedClusteringBuilder<Bvh, uint32>;

    auto [bboxes, centers] = bvh::compute_bounding_boxes_and_centers(primitives.data(), primitives.size());
    auto global_bbox       = bvh::compute_bounding_boxes_union(bboxes.get(), primitives.size());

    Bvh bvh;
    BvhBuilder builder(bvh);
    builder.build(global_bbox, bboxes.get(), centers.get(), primitives.size());

    bvh::ParallelReinsertionOptimizer parallel_optimizer(bvh);
    parallel_optimizer.optimize();

    bvh::LeafCollapser leaf_optimizer(bvh);
    leaf_optimizer.collapse();

    bvh::NodeLayoutOptimizer layout_optimizer(bvh);
    layout_optimizer.optimize();

    BvhNEntAdapter<N, Allocator> adapter(nodes, objs);
    adapter.adapt(bvh, primitives);
}
} // namespace IG