#pragma once

#include "NArityBvh.h"

#include <ranges>

namespace IG {
template <size_t N, typename Node, std::ranges::random_access_range PrimitiveRange>
class BvhNAdapter {
public:
    using Bvh = NArityBvh<N>;

    BvhNAdapter(std::vector<Node>& nodes, const PrimitiveRange& primitives)
        : nodes(nodes)
        , primitives(primitives)
    {
    }

    void adapt(const bvh::Bvh& bvh)
    {
        Bvh new_bvh;
        convert_to_narity(bvh, new_bvh);
        // new_bvh.dump(std::cout);

        // Special case when root is already the leaf
        if (new_bvh.nodes[0].is_leaf()) {
            const auto old_node = new_bvh.nodes[0];
            const auto new_node = typename Bvh::Node{ { old_node.bounds[0], old_node.bounds[1], old_node.bounds[2], old_node.bounds[3], old_node.bounds[4], old_node.bounds[5] }, 1, 0 };
            write_node(new_bvh, new_node, -1, 0);
        } else {
            write_node(new_bvh, new_bvh.nodes[0], -1, 0);
        }
    }

protected:
    static inline size_t primitive_count_of_node(const typename Bvh::Node& node) { return node.primitive_count(); }

    void write_node(const Bvh& bvh,
                    const typename Bvh::Node& node,
                    int parent,
                    size_t child)
    {
        IG_ASSERT(!node.is_leaf(), "Expected a node");
        IG_ASSERT(node.primitive_or_child_count > 0 && node.primitive_or_child_count <= (int32)N, "Invalid node with no children");
        IG_ASSERT(node.first_child_or_primitive < bvh.nodes.size(), "Invalid child with malformed indices");

        const size_t node_id = nodes.size();
        if (parent >= 0)
            nodes[parent].child.e[child] = (int)node_id + 1; // The id is shifted by one
        nodes.emplace_back();

        for (size_t i = 0; i < (size_t)node.primitive_or_child_count; ++i) {
            const auto& src_node = bvh.nodes.at(node.first_child_or_primitive + i);
            {
                auto& dst = nodes.at(node_id);
                for (int k = 0; k < 6; ++k) {
                    if constexpr (N > 2) {
                        dst.bounds.e[k].e[i] = src_node.bounds[k];
                    } else { // FIXME: Get rid of this difference
                        dst.bounds.e[k + i * 6] = src_node.bounds[k];
                    }
                }
            }

            if (src_node.is_leaf())
                write_leaf(bvh, src_node, (int)node_id, i);
            else
                write_node(bvh, src_node, (int)node_id, i);
        }

        for (size_t i = (size_t)node.primitive_or_child_count; i < N; ++i) {
            auto& dst = nodes.at(node_id);

            if constexpr (N > 2) {
                dst.bounds.e[0].e[i] = FltInf;
                dst.bounds.e[2].e[i] = FltInf;
                dst.bounds.e[4].e[i] = FltInf;

                dst.bounds.e[1].e[i] = -FltInf;
                dst.bounds.e[3].e[i] = -FltInf;
                dst.bounds.e[5].e[i] = -FltInf;
            } else { // FIXME: Get rid of this difference
                dst.bounds.e[0 + i * 6] = FltInf;
                dst.bounds.e[2 + i * 6] = FltInf;
                dst.bounds.e[4 + i * 6] = FltInf;

                dst.bounds.e[1 + i * 6] = -FltInf;
                dst.bounds.e[3 + i * 6] = -FltInf;
                dst.bounds.e[5 + i * 6] = -FltInf;
            }

            dst.child.e[i] = 0;
        }
    }

    virtual void write_leaf(const Bvh& bvh,
                            const typename Bvh::Node& node,
                            size_t parent,
                            size_t child)
        = 0;

    std::vector<Node>& nodes;
    PrimitiveRange primitives;
};
} // namespace IG