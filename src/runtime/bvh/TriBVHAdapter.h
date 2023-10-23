#pragma once

#include "BvhNAdapter.h"
#include "math/Triangle.h"
#include "mesh/TriMesh.h"

#include <ranges>

IG_BEGIN_IGNORE_WARNINGS
#include <bvh/v2/default_builder.h>
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

inline bvh::Vec3 compute_stable_triangle_normal(const bvh::Vec3& a, const bvh::Vec3& b, const bvh::Vec3& c)
{
    const float ab_x = a[2] * b[1], ab_y = a[0] * b[2], ab_z = a[1] * b[0];
    const float bc_x = b[2] * c[1], bc_y = b[0] * c[2], bc_z = b[1] * c[0];
    const bvh::Vec3 cross_ab(a[1] * b[2] - ab_x, a[2] * b[0] - ab_y, a[0] * b[1] - ab_z);
    const bvh::Vec3 cross_bc(b[1] * c[2] - bc_x, b[2] * c[0] - bc_y, b[0] * c[1] - bc_z);
    return bvh::Vec3(abs(ab_x) < abs(bc_x) ? cross_ab[0] : cross_bc[0],
                     abs(ab_y) < abs(bc_y) ? cross_ab[1] : cross_bc[1],
                     abs(ab_z) < abs(bc_z) ? cross_ab[2] : cross_bc[2]);
}

struct TriangleProxy {
    bvh::Vec3 p0, e1, e2, n;

    TriangleProxy() = default;
    TriangleProxy(const bvh::Vec3& p0, const bvh::Vec3& p1, const bvh::Vec3& p2)
        : p0(p0)
        , e1(p2 - p0)
        , e2(p0 - p1)
    {
        n = compute_stable_triangle_normal(e1, e2, p1 - p2);
    }

    bvh::Vec3 p1() const { return p0 - e2; }
    bvh::Vec3 p2() const { return p0 + e1; }

    bvh::Bbox bounding_box() const
    {
        return bvh::Bbox(p0).extend(p1()).extend(p2());
    }

    bvh::Vec3 center() const
    {
        return (p0 + p1() + p2()) * (1.0f / 3);
    }
};

template <size_t N, size_t M, std::ranges::random_access_range PrimitiveRange>
class BvhNTriMAdapter : public BvhNAdapter<N, typename BvhNTriM<N, M>::Node, PrimitiveRange> {
    using Parent = BvhNAdapter<N, typename BvhNTriM<N, M>::Node, PrimitiveRange>;
    using Bvh    = typename Parent::Bvh;
    using Node   = typename BvhNTriM<N, M>::Node;
    using Tri    = typename BvhNTriM<N, M>::Tri;

    std::vector<Tri>& tris;

public:
    BvhNTriMAdapter(std::vector<Node>& nodes, const PrimitiveRange& primitives, std::vector<Tri>& tris)
        : Parent(nodes, primitives)
        , tris(tris)
    {
    }

protected:
    virtual void write_leaf(const Bvh& bvh,
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
                const int id      = (int)bvh.primitive_indices.at(node.first_child_or_primitive + i + j);
                const auto in_tri = std::ranges::cbegin(this->primitives)[id];

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

                tri.prim_id.e[j] = id;
            }

            for (size_t j = c; j < M; ++j)
                tri.prim_id.e[j] = 0xFFFFFFFF;

            tris.emplace_back(tri);
        }

        tris.back().prim_id.e[M - 1] |= 0x80000000;
    }
};

template <std::ranges::random_access_range PrimitiveRange>
class BvhNTriMAdapter<2, 1, PrimitiveRange> : public BvhNAdapter<2, Node2, PrimitiveRange> {
    using Parent = BvhNAdapter<2, Node2, PrimitiveRange>;
    using Bvh    = typename Parent::Bvh;
    using Node   = Node2;
    using Tri    = Tri1;

    std::vector<Tri>& tris;

public:
    BvhNTriMAdapter(std::vector<Node>& nodes, const PrimitiveRange& primitives, std::vector<Tri>& tris)
        : Parent(nodes, primitives)
        , tris(tris)
    {
    }

protected:
    virtual void write_leaf(const Bvh& bvh,
                            const typename Bvh::Node& node,
                            size_t parent,
                            size_t child) override
    {
        IG_ASSERT(node.is_leaf(), "Expected a leaf");

        this->nodes[parent].child.e[child] = ~static_cast<int>(tris.size());

        for (size_t i = 0; i < this->primitive_count_of_node(node); ++i) {
            const int id      = (int)bvh.primitive_indices.at(node.first_child_or_primitive + i);
            const auto in_tri = std::ranges::cbegin(this->primitives)[id];
            tris.emplace_back(Tri1{
                { in_tri.p0[0], in_tri.p0[1], in_tri.p0[2] },
                0,
                { in_tri.e1[0], in_tri.e1[1], in_tri.e1[2] },
                0,
                { in_tri.e2[0], in_tri.e2[1], in_tri.e2[2] },
                id});
        }

        // Add sentinel
        tris.back().prim_id |= 0x80000000;
    }
};

template <size_t N, size_t M, std::ranges::random_access_range PrimitiveRange>
static inline BvhNTriMAdapter<N, M, PrimitiveRange> make_bvh_adapter(std::vector<typename BvhNTriM<N, M>::Node>& nodes,
                                                                     const PrimitiveRange& primitives,
                                                                     std::vector<typename BvhNTriM<N, M>::Tri>& tris)
{
    return BvhNTriMAdapter<N, M, PrimitiveRange>(nodes, primitives, tris);
}

template <size_t N, size_t M>
inline void build_bvh(const TriMesh& tri_mesh,
                      std::vector<typename BvhNTriM<N, M>::Node>& nodes,
                      std::vector<typename BvhNTriM<N, M>::Tri>& tris)
{
    // FIXME: No need for this anymore
    const auto triangles = std::views::iota((size_t)0, (size_t)tri_mesh.faceCount()) | std::views::transform([&](size_t index) -> const TriangleProxy {
                               const auto v0 = bvh::from(tri_mesh.vertices.at(tri_mesh.indices.at(index * 4 + 0)));
                               const auto v1 = bvh::from(tri_mesh.vertices.at(tri_mesh.indices.at(index * 4 + 1)));
                               const auto v2 = bvh::from(tri_mesh.vertices.at(tri_mesh.indices.at(index * 4 + 2)));
                               return TriangleProxy(v0, v1, v2);
                           });

    const auto bboxes_r  = triangles | std::views::transform([](const TriangleProxy& t) -> const bvh::Bbox { return t.bounding_box(); });
    const auto centers_r = triangles | std::views::transform([](const TriangleProxy& t) -> const bvh::Vec3 { return t.center(); });

    // FIXME: libbvh expects a span which is a continuous view, but range returns a random-access-view
    // Might require changes in libbvh
    const std::vector<bvh::Bbox> bboxes(bboxes_r.begin(), bboxes_r.end());
    const std::vector<bvh::Vec3> centers(centers_r.begin(), centers_r.end());

    using Builder = ::bvh::v2::DefaultBuilder<bvh::Node>;
    typename Builder::Config config;
    config.quality       = Builder::Quality::High;
    config.max_leaf_size = M;
    auto bvh             = Builder::build(bboxes, centers);

    make_bvh_adapter<N, M>(nodes, triangles, tris).adapt(bvh);
}
} // namespace IG
