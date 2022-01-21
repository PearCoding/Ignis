#pragma once

#include "NArityBvh.h"
#include "bvh/bvh.hpp"

namespace IG {
template <size_t N, typename Node, typename LeafObject, template <typename> typename Allocator>
class BvhNAdapter {
public:
    using Bvh = NArityBvh<N>;

    BvhNAdapter(std::vector<Node, Allocator<Node>>& nodes)
        : nodes(nodes)
    {
    }

    void adapt(const bvh::Bvh<float>& bvh, const std::vector<LeafObject>& primitives)
    {
        NArityBvh<N> new_bvh;
        convert_to_narity(bvh, new_bvh);

        // Special case when root is already the leaf
        if (new_bvh.nodes[0].is_leaf()) {
            const auto old_node = new_bvh.nodes[0];
            const auto new_node = typename NArityBvh<N>::Node{ { old_node.bounds[0], old_node.bounds[1], old_node.bounds[2], old_node.bounds[3], old_node.bounds[4], old_node.bounds[5] }, 1, 0 };
            write_node(primitives, new_bvh, new_node, -1, 0);
        } else {
            write_node(primitives, new_bvh, new_bvh.nodes[0], -1, 0);
        }
    }

protected:
    size_t primitive_count_of_node(const typename Bvh::Node& node) { return node.primitive_count(); }

    void write_node(const std::vector<LeafObject>& primitives,
                    const Bvh& bvh,
                    const typename Bvh::Node& node,
                    int parent,
                    size_t child)
    {
        IG_ASSERT(!node.is_leaf(), "Expected a node");
        IG_ASSERT(node.primitive_or_child_count > 0 && node.primitive_or_child_count <= (int32)N, "Invalid node with no children");
        IG_ASSERT(node.first_child_or_primitive < bvh.nodes.size(), "Invalid child with malformed indices");

        const size_t node_id = nodes.size();
        if (parent >= 0)
            nodes[parent].child.e[child] = node_id + 1; // The id is shifted by one
        nodes.emplace_back();

        for (size_t i = 0; i < (size_t)node.primitive_or_child_count; ++i) {
            const auto& src_node = bvh.nodes[node.first_child_or_primitive + i];
            for (int k = 0; k < 6; ++k)
                nodes[node_id].bounds.e[k].e[i] = src_node.bounds[k];

            if (src_node.is_leaf())
                write_leaf(primitives, bvh, src_node, node_id, i);
            else
                write_node(primitives, bvh, src_node, node_id, i);
        }

        for (size_t i = (size_t)node.primitive_or_child_count; i < N; ++i) {
            auto& dst = nodes[node_id];

            dst.bounds.e[0].e[i] = FltInf;
            dst.bounds.e[2].e[i] = FltInf;
            dst.bounds.e[4].e[i] = FltInf;

            dst.bounds.e[1].e[i] = -FltInf;
            dst.bounds.e[3].e[i] = -FltInf;
            dst.bounds.e[5].e[i] = -FltInf;

            dst.child.e[i] = 0;
        }
    }

    virtual void write_leaf(const std::vector<LeafObject>& primitives,
                            const Bvh& bvh,
                            const typename Bvh::Node& node,
                            size_t parent,
                            size_t child)
        = 0;

    std::vector<Node, Allocator<Node>>& nodes;
};

template <typename Node, typename LeafObject, template <typename> typename Allocator>
class BvhNAdapter<2, Node, LeafObject, Allocator> {
public:
    using Bvh = bvh::Bvh<float>;

    BvhNAdapter(std::vector<Node, Allocator<Node>>& nodes)
        : nodes(nodes)
    {
    }

    void adapt(const Bvh& bvh, const std::vector<LeafObject>& primitives)
    {
        // Special case when root is already the leaf
        if (bvh.nodes[0].is_leaf()) {
            const auto old      = bvh.nodes[0];
            const auto new_node = typename Bvh::Node{ { old.bounds[0], old.bounds[1], old.bounds[2], old.bounds[3], old.bounds[4], old.bounds[5] }, 0, 0 };
            write_node(primitives, bvh, new_node, -1, 0);
        } else {
            write_node(primitives, bvh, bvh.nodes[0], -1, 0);
        }
    }

protected:
    size_t primitive_count_of_node(const typename Bvh::Node& node) { return node.primitive_count; }

    void write_node(const std::vector<LeafObject>& primitives,
                    const Bvh& bvh,
                    const typename Bvh::Node& node,
                    int parent,
                    size_t child)
    {
        IG_ASSERT(!node.is_leaf(), "Expected a node");
        IG_ASSERT(node.first_child_or_primitive < bvh.node_count, "Invalid child with malformed indices");

        const size_t node_id = nodes.size();
        if (parent >= 0)
            nodes[parent].child.e[child] = node_id + 1; // The id is shifted by one
        nodes.emplace_back();

        for (size_t i = 0; i < 2; ++i) {
            const size_t id = node.first_child_or_primitive + i;
            if (id < bvh.node_count) {
                const auto& src_node = bvh.nodes[id];
                for (int k = 0; k < 6; ++k)
                    nodes[node_id].bounds.e[k + i * 6] = src_node.bounds[k];

                if (src_node.is_leaf())
                    write_leaf(primitives, bvh, src_node, node_id, i);
                else
                    write_node(primitives, bvh, src_node, node_id, i);
            } else {
                // For some reason the child is cut out, make it invalid
                auto& dst = nodes[node_id];

                dst.bounds.e[0 + i * 6] = FltInf;
                dst.bounds.e[2 + i * 6] = FltInf;
                dst.bounds.e[4 + i * 6] = FltInf;

                dst.bounds.e[1 + i * 6] = -FltInf;
                dst.bounds.e[3 + i * 6] = -FltInf;
                dst.bounds.e[5 + i * 6] = -FltInf;

                dst.child.e[i] = 0;
            }
        }
    }

    virtual void write_leaf(const std::vector<LeafObject>& primitives,
                            const Bvh& bvh,
                            const typename Bvh::Node& node,
                            size_t parent,
                            size_t child)
        = 0;

    std::vector<Node, Allocator<Node>>& nodes;
};
} // namespace IG