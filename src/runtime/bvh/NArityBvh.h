#pragma once

#include "Bvh.h"

#include <queue>

namespace IG {

template <size_t N>
struct NArityBvh {
    struct Node {
        std::array<float, 6> bounds;
        int32 primitive_or_child_count;
        uint32 first_child_or_primitive;

        inline bool is_leaf() const { return primitive_or_child_count < 0; }
        inline uint32 primitive_count() const { return (uint32)-primitive_or_child_count; }
    };

    std::vector<Node> nodes;
    std::vector<size_t> primitive_indices;

    void dump(std::ostream& os)
    {
        os << "digraph {" << std::endl
           << "label=\"" << nodes.size() << " Nodes, " << primitive_indices.size() << " Prims\";" << std::endl;
        dump(0, os);
        os << "}" << std::endl;
    }

private:
    void dump(size_t index, std::ostream& os)
    {
        const auto& node = nodes.at(index);

        if (!node.is_leaf()) {
            os << "N" << index << " [shape=square,style=filled,color=\"lightblue\"];" << std::endl
               << "N" << index << " -> { ";
            for (uint32 i = 0; i < (uint32)node.primitive_or_child_count; ++i) {
                size_t child_index = node.first_child_or_primitive + i;
                if (nodes.at(child_index).is_leaf())
                    os << "L" << child_index;
                else
                    os << "N" << child_index;

                os << " ";
            }

            os << "};" << std::endl;

            for (uint32 i = 0; i < (uint32)node.primitive_or_child_count; ++i)
                dump(node.first_child_or_primitive + i, os);
        } else {
            os << "L" << index << " [shape=hexagon,style=filled,color=\"lightsalmon\"];" << std::endl
               << "L" << index << " -> { ";
            for (uint32 i = 0; i < (uint32)node.primitive_count(); ++i) {
                os << "P" << (i + node.first_child_or_primitive) << "[style=filled,color=\"limegreen\"] ";
            }
            os << "};" << std::endl;
        }
    }
};

template <size_t N>
inline typename NArityBvh<N>::Node clone_node(const bvh::Node& original)
{
    return typename NArityBvh<N>::Node{
        .bounds                   = original.bounds, // Same ordering as libbvh
        .primitive_or_child_count = original.is_leaf() ? -static_cast<int32>(original.index.prim_count) : (int32)N,
        .first_child_or_primitive = original.index.first_id,
    };
}

inline constexpr int uint32_log2(uint32_t n)
{
#define S(k)                       \
    if (n >= (UINT32_C(1) << k)) { \
        i += k;                    \
        n >>= k;                   \
    }

    int i = -(n == 0);
    S(16);
    S(8);
    S(4);
    S(2);
    S(1);
    return i;

#undef S
}

template <size_t N>
inline void convert_to_narity_node(const bvh::Bvh& original, const bvh::Node& node, NArityBvh<N>& bvh, uint32 cur_id)
{
    constexpr size_t MaxIter = (size_t)1 << (uint32_log2(N) - 1);

    if (!node.is_leaf()) {
        std::queue<bvh::Node> queue;
        queue.push(original.nodes[node.index.first_id + 0]);
        queue.push(original.nodes[node.index.first_id + 1]);

        std::vector<bvh::Node> children;
        for (size_t k = 0; k < MaxIter && !queue.empty(); ++k) {
            const auto cur = queue.front();
            queue.pop();
            if (cur.is_leaf())
                children.push_back(cur);
            else {
                queue.push(original.nodes[cur.index.first_id + 0]);
                queue.push(original.nodes[cur.index.first_id + 1]);
            }
        }

        // Flush out the whole queue
        while (!queue.empty()) {
            children.push_back(queue.front());
            queue.pop();
        }

        // Setup new indices
        bvh.nodes[cur_id].primitive_or_child_count = (int)children.size();
        bvh.nodes[cur_id].first_child_or_primitive = (uint32)bvh.nodes.size();
        for (auto& child : children)
            bvh.nodes.push_back(clone_node<N>(child));

        // Take care of potential children children
        for (size_t i = 0; i < children.size(); ++i) {
            convert_to_narity_node(original, children[i], bvh, bvh.nodes[cur_id].first_child_or_primitive + (uint32)i);
        }
    }
}

template <size_t N>
inline void convert_to_narity(const bvh::Bvh& original, NArityBvh<N>& bvh)
{
    bvh.nodes.reserve((original.nodes.size() - 1) / (N / 2));

    bvh.nodes.push_back(clone_node<N>(original.nodes[0]));
    convert_to_narity_node(original, original.nodes[0], bvh, 0);

    bvh.primitive_indices = original.prim_ids;
}

template <>
inline void convert_to_narity(const bvh::Bvh& original, NArityBvh<2>& bvh)
{
    // Just copy pasta
    bvh.nodes.resize(original.nodes.size());

    for (size_t i = 0; i < original.nodes.size(); ++i)
        bvh.nodes[i] = clone_node<2>(original.nodes[i]);

    bvh.primitive_indices = original.prim_ids;
}
} // namespace IG