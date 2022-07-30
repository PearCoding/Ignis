#pragma once

#include "BvhNAdapter.h"
#include "Target.h"
#include "math/Triangle.h"
#include "mesh/TriMesh.h"

#include "Image.h"

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

struct TriangleProxy : public bvh::Triangle<float> {
    using bvh::Triangle<float>::Triangle;
    Vector2f c0, c1, c2;
    int prim_id;
};

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

                tri.c0.e[0].e[j] = in_tri.c0[0];
                tri.c0.e[1].e[j] = in_tri.c0[1];

                tri.c1.e[0].e[j] = in_tri.c1[0];
                tri.c1.e[1].e[j] = in_tri.c1[1];

                tri.c2.e[0].e[j] = in_tri.c2[0];
                tri.c2.e[1].e[j] = in_tri.c2[1];
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
                in_tri.prim_id,
                { in_tri.c0[0], in_tri.c0[1] }, //c0
                { in_tri.c1[0], in_tri.c1[1] }, //c1
                { in_tri.c2[0], in_tri.c2[1] }, //c2
                {0,0} // additional padding
                });
        }

        // Add sentinel
        tris.back().prim_id |= 0x80000000;
    }
};

float disp(float x) {
    if (x > 0.5) return 1;
    return 0;
}

template <size_t N, size_t M, template <typename> typename Allocator>
inline void build_bvh(const TriMesh& tri_mesh,
                      std::vector<typename BvhNTriM<N, M>::Node, Allocator<typename BvhNTriM<N, M>::Node>>& nodes,
                      std::vector<typename BvhNTriM<N, M>::Tri, Allocator<typename BvhNTriM<N, M>::Tri>>& tris)
{
    using Bvh        = bvh::Bvh<float>;
    using BvhBuilder = bvh::SpatialSplitBvhBuilder<Bvh, TriangleProxy, 64>;
    // using BvhBuilder = bvh::LocallyOrderedClusteringBuilder<Bvh, uint32>;
    // using BvhBuilder = bvh::SweepSahBuilder<Bvh>;

    Image disp_tex = Image::load("../scenes/textures/bumpmap.png");

    const size_t num_tris = tri_mesh.indices.size() / 4;
    std::vector<TriangleProxy> primitives(num_tris);
    for (size_t i = 0; i < num_tris; ++i) {
        auto& v0      = tri_mesh.vertices[tri_mesh.indices[i * 4 + 0]];
        auto& v1      = tri_mesh.vertices[tri_mesh.indices[i * 4 + 1]];
        auto& v2      = tri_mesh.vertices[tri_mesh.indices[i * 4 + 2]];

        auto& c0      = tri_mesh.texcoords[tri_mesh.indices[i * 4 + 0]];
        auto& c1      = tri_mesh.texcoords[tri_mesh.indices[i * 4 + 1]];
        auto& c2      = tri_mesh.texcoords[tri_mesh.indices[i * 4 + 2]];

        auto tri = TriangleProxy(v0, v1, v2);
        tri.c0 = c0;
        tri.c1 = c1;
        tri.c2 = c2;

        primitives[i] = tri;

        primitives[i].prim_id = (int)i;
    }

    auto [bboxes, centers] = bvh::compute_bounding_boxes_and_centers(primitives.data(), primitives.size());
    auto global_bbox       = bvh::compute_bounding_boxes_union(bboxes.get(), primitives.size());

    for (size_t i = 0; i < num_tris; ++i) {
        TriangleProxy t = primitives[i];

        auto normal = normalize(cross(t.e1, t.e2));

        auto& c0      = tri_mesh.texcoords[tri_mesh.indices[i * 4 + 0]];
        auto& c1      = tri_mesh.texcoords[tri_mesh.indices[i * 4 + 1]];
        auto& c2      = tri_mesh.texcoords[tri_mesh.indices[i * 4 + 2]];

        int id0 = (int)floor(c0.x() * disp_tex.width * disp_tex.height + c0.y() * disp_tex.height) * 4;   
        int id1 = (int)floor(c1.x() * disp_tex.width * disp_tex.height + c1.y() * disp_tex.height) * 4; 
        int id2 = (int)floor(c2.x() * disp_tex.width * disp_tex.height + c2.y() * disp_tex.height) * 4; 

        float d0 = disp_tex.pixels[id0];
        float d1 = disp_tex.pixels[id1];
        float d2 = disp_tex.pixels[id2];

        
        primitives[i].c0 = c0;
        primitives[i].c1 = c1;
        primitives[i].c2 = c2;

        bvh::BoundingBox<float> bb;
        bb.extend(t.p0  );
        bb.extend(t.p1());
        bb.extend(t.p2());
        bb.extend(t.p0   + normal * 1.01f);
        bb.extend(t.p1() + normal * 1.01f);
        bb.extend(t.p2() + normal * 1.01f);

        bboxes[i] = bb;

    }

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