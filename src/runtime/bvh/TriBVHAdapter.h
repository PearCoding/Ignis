#pragma once

#include "BvhNAdapter.h"
#include "math/Triangle.h"
#include "mesh/TriMesh.h"

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

// Contains implementation for NodeN and TriN
#include "generated_interface.h"

namespace IG {

template <size_t N, size_t M>
struct BvhNTriM {
};

template <>
struct BvhNTriM<8, 4> {
    using Node = Node8;
    using Tri  = Tri4;
};

template <>
struct BvhNTriM<4, 4> {
    using Node = Node4;
    using Tri  = Tri4;
};

template <>
struct BvhNTriM<2, 1> {
    using Node = Node2;
    using Tri  = Tri1;
};

template <typename Scalar>
inline bvh::Vector3<Scalar> compute_stable_triangle_normal(const bvh::Vector3<Scalar>& a, const bvh::Vector3<Scalar>& b, const bvh::Vector3<Scalar>& c)
{
    const Scalar ab_x = a[2] * b[1], ab_y = a[0] * b[2], ab_z = a[1] * b[0];
    const Scalar bc_x = b[2] * c[1], bc_y = b[0] * c[2], bc_z = b[1] * c[0];
    const bvh::Vector3<Scalar> cross_ab(a[1] * b[2] - ab_x, a[2] * b[0] - ab_y, a[0] * b[1] - ab_z);
    const bvh::Vector3<Scalar> cross_bc(b[1] * c[2] - bc_x, b[2] * c[0] - bc_y, b[0] * c[1] - bc_z);
    return bvh::Vector3<Scalar>(abs(ab_x) < abs(bc_x) ? cross_ab[0] : cross_bc[0],
                                abs(ab_y) < abs(bc_y) ? cross_ab[1] : cross_bc[1],
                                abs(ab_z) < abs(bc_z) ? cross_ab[2] : cross_bc[2]);
}

template <typename Scalar>
struct TriangleProxyBase {
    using ScalarType = Scalar;

    bvh::Vector3<Scalar> p0, e1, e2, n;
    int32 prim_id;

    TriangleProxyBase() = default;
    TriangleProxyBase(const bvh::Vector3<Scalar>& p0, const bvh::Vector3<Scalar>& p1, const bvh::Vector3<Scalar>& p2)
        : p0(p0)
        , e1(p2 - p0)
        , e2(p0 - p1)
    {
        n = compute_stable_triangle_normal(e1, e2, p1 - p2);
    }

    bvh::Vector3<Scalar> p1() const { return p0 - e2; }
    bvh::Vector3<Scalar> p2() const { return p0 + e1; }

    bvh::BoundingBox<Scalar> bounding_box() const
    {
        bvh::BoundingBox<Scalar> bbox(p0);
        bbox.extend(p1());
        bbox.extend(p2());
        return bbox;
    }

    bvh::Vector3<Scalar> center() const
    {
        return (p0 + p1() + p2()) * (Scalar(1.0) / Scalar(3.0));
    }

    std::pair<bvh::Vector3<Scalar>, bvh::Vector3<Scalar>> edge(size_t i) const
    {
        assert(i < 3);
        bvh::Vector3<Scalar> p[] = { p0, p1(), p2() };
        return std::make_pair(p[i], p[(i + 1) % 3]);
    }

    Scalar area() const
    {
        return length(n) * Scalar(0.5);
    }

    std::pair<bvh::BoundingBox<Scalar>, bvh::BoundingBox<Scalar>> split(size_t axis, Scalar position) const
    {
        bvh::Vector3<Scalar> p[] = { p0, p1(), p2() };
        auto left                = bvh::BoundingBox<Scalar>::empty();
        auto right               = bvh::BoundingBox<Scalar>::empty();
        auto split_edge          = [=](const bvh::Vector3<Scalar>& a, const bvh::Vector3<Scalar>& b) {
            auto t = (position - a[axis]) / (b[axis] - a[axis]);
            return a + t * (b - a);
        };
        auto q0 = p[0][axis] <= position;
        auto q1 = p[1][axis] <= position;
        auto q2 = p[2][axis] <= position;
        if (q0)
            left.extend(p[0]);
        else
            right.extend(p[0]);
        if (q1)
            left.extend(p[1]);
        else
            right.extend(p[1]);
        if (q2)
            left.extend(p[2]);
        else
            right.extend(p[2]);
        if (q0 ^ q1) {
            auto m = split_edge(p[0], p[1]);
            left.extend(m);
            right.extend(m);
        }
        if (q1 ^ q2) {
            auto m = split_edge(p[1], p[2]);
            left.extend(m);
            right.extend(m);
        }
        if (q2 ^ q0) {
            auto m = split_edge(p[2], p[0]);
            left.extend(m);
            right.extend(m);
        }
        return std::make_pair(left, right);
    }
};
using TriangleProxy = TriangleProxyBase<float>;

template <size_t N, size_t M, template <typename> typename Allocator>
class BvhNTriMAdapter : public BvhNAdapter<N, typename BvhNTriM<N, M>::Node, TriangleProxy, Allocator> {
    using Parent = BvhNAdapter<N, typename BvhNTriM<N, M>::Node, TriangleProxy, Allocator>;
    using Bvh    = typename Parent::Bvh;
    using Node   = typename BvhNTriM<N, M>::Node;
    using Tri    = typename BvhNTriM<N, M>::Tri;

    std::vector<Tri, Allocator<Tri>>& tris;

public:
    BvhNTriMAdapter(std::vector<Node, Allocator<Node>>& nodes, std::vector<Tri, Allocator<Tri>>& tris)
        : Parent(nodes)
        , tris(tris)
    {
    }

protected:
    virtual void write_leaf(const std::vector<TriangleProxy>& primitives,
                            const Bvh& bvh,
                            const typename Bvh::Node& node,
                            size_t parent,
                            size_t child) override
    {
        IG_ASSERT(node.is_leaf(), "Expected a leaf");

        this->nodes[parent].child.e[child] = ~static_cast<int>(tris.size());

        const size_t ref_count = this->primitive_count_of_node(node);

        // Group triangles by packets of M
        for (size_t i = 0; i < ref_count; i += M) {
            const size_t c = i + M <= ref_count ? M : ref_count - i;

            Tri tri;
            std::memset(&tri, 0, sizeof(Tri));
            for (size_t j = 0; j < c; ++j) {
                const int id       = (int)bvh.primitive_indices[node.first_child_or_primitive + i + j];
                const auto& in_tri = primitives[id];

                tri.v0.e[0].e[j] = in_tri.p0[0];
                tri.v0.e[1].e[j] = in_tri.p0[1];
                tri.v0.e[2].e[j] = in_tri.p0[2];

                tri.e1.e[0].e[j] = in_tri.e1[0];
                tri.e1.e[1].e[j] = in_tri.e1[1];
                tri.e1.e[2].e[j] = in_tri.e1[2];

                tri.e2.e[0].e[j] = in_tri.e2[0];
                tri.e2.e[1].e[j] = in_tri.e2[1];
                tri.e2.e[2].e[j] = in_tri.e2[2];

                tri.n.e[0].e[j] = in_tri.n[0];
                tri.n.e[1].e[j] = in_tri.n[1];
                tri.n.e[2].e[j] = in_tri.n[2];

                tri.prim_id.e[j] = in_tri.prim_id;
            }

            for (size_t j = c; j < M; ++j)
                tri.prim_id.e[j] = 0xFFFFFFFF;

            tris.emplace_back(tri);
        }

        tris.back().prim_id.e[M - 1] |= 0x80000000;
    }
};

template <template <typename> typename Allocator>
class BvhNTriMAdapter<2, 1, Allocator> : public BvhNAdapter<2, typename BvhNTriM<2, 1>::Node, TriangleProxy, Allocator> {
    using Parent = BvhNAdapter<2, typename BvhNTriM<2, 1>::Node, TriangleProxy, Allocator>;
    using Bvh    = typename Parent::Bvh;
    using Node   = Node2;
    using Tri    = Tri1;

    std::vector<Tri, Allocator<Tri>>& tris;

public:
    BvhNTriMAdapter(std::vector<Node, Allocator<Node>>& nodes, std::vector<Tri, Allocator<Tri>>& tris)
        : Parent(nodes)
        , tris(tris)
    {
    }

protected:
    virtual void write_leaf(const std::vector<TriangleProxy>& primitives,
                            const Bvh& bvh,
                            const typename Bvh::Node& node,
                            size_t parent,
                            size_t child) override
    {
        IG_ASSERT(node.is_leaf(), "Expected a leaf");

        this->nodes[parent].child.e[child] = ~static_cast<int>(tris.size());

        for (size_t i = 0; i < this->primitive_count_of_node(node); ++i) {
            const int id = (int)bvh.primitive_indices[node.first_child_or_primitive + i];
            auto& in_tri = primitives[id];
            tris.emplace_back(Tri1{
                { in_tri.p0[0], in_tri.p0[1], in_tri.p0[2] },
                0,
                { in_tri.e1[0], in_tri.e1[1], in_tri.e1[2] },
                0,
                { in_tri.e2[0], in_tri.e2[1], in_tri.e2[2] },
                in_tri.prim_id });
        }

        // Add sentinel
        tris.back().prim_id |= 0x80000000;
    }
};

template <size_t N, size_t M, template <typename> typename Allocator>
inline void build_bvh(const TriMesh& tri_mesh,
                      std::vector<typename BvhNTriM<N, M>::Node, Allocator<typename BvhNTriM<N, M>::Node>>& nodes,
                      std::vector<typename BvhNTriM<N, M>::Tri, Allocator<typename BvhNTriM<N, M>::Tri>>& tris)
{
    using Bvh        = bvh::Bvh<float>;
    using BvhBuilder = bvh::SpatialSplitBvhBuilder<Bvh, TriangleProxy, 64>;
    // using BvhBuilder = bvh::LocallyOrderedClusteringBuilder<Bvh, uint32>;
    // using BvhBuilder = bvh::SweepSahBuilder<Bvh>;

    const size_t num_tris = tri_mesh.faceCount();
    std::vector<TriangleProxy> primitives(num_tris);
    for (size_t i = 0; i < num_tris; ++i) {
        auto& v0      = tri_mesh.vertices.at(tri_mesh.indices.at(i * 4 + 0));
        auto& v1      = tri_mesh.vertices.at(tri_mesh.indices.at(i * 4 + 1));
        auto& v2      = tri_mesh.vertices.at(tri_mesh.indices.at(i * 4 + 2));
        primitives[i] = TriangleProxy(v0, v1, v2);

        primitives[i].prim_id = (int32)i;
    }

    auto [bboxes, centers] = bvh::compute_bounding_boxes_and_centers(primitives.data(), primitives.size());
    auto global_bbox       = bvh::compute_bounding_boxes_union(bboxes.get(), primitives.size());

    Bvh bvh;
    BvhBuilder builder(bvh);
    builder.build(global_bbox, primitives.data(), bboxes.get(), centers.get(), primitives.size());
    // builder.build(global_bbox, bboxes.get(), centers.get(), primitives.size());

    bvh::ParallelReinsertionOptimizer parallel_optimizer(bvh);
    parallel_optimizer.optimize();

    // bvh::LeafCollapser leaf_optimizer(bvh);
    // leaf_optimizer.collapse();

    bvh::NodeLayoutOptimizer layout_optimizer(bvh);
    layout_optimizer.optimize();

    BvhNTriMAdapter<N, M, Allocator> adapter(nodes, tris);
    adapter.adapt(bvh, primitives);
}
} // namespace IG