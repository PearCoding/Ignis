#pragma once

#include "bvh/bvh.hpp"

namespace IG {
template <size_t N>
struct NArityBvh {
    struct Node {
        float bounds[6];
        int32 primitive_or_child_count;
        uint32 first_child_or_primitive;

        inline bool is_leaf() const { return primitive_or_child_count < 0; }
        inline uint32 primitive_count() const { return (uint32)-primitive_or_child_count; }
    };

    std::vector<Node> nodes;
    std::vector<size_t> primitive_indices;
};

template <size_t N>
inline typename NArityBvh<N>::Node clone_node(const bvh::Bvh<float>::Node& original)
{
    typename NArityBvh<N>::Node node;
    for (int i = 0; i < 6; ++i)
        node.bounds[i] = original.bounds[i];
    node.primitive_or_child_count = -static_cast<int32>(original.primitive_count);
    node.first_child_or_primitive = original.first_child_or_primitive;
    return node;
}

template <typename Scalar>
inline size_t primitive_count(const bvh::Bvh<Scalar>& bvh)
{
    size_t n = 0;
    for (size_t i = 0; i < bvh.node_count; ++i)
        n += bvh.nodes[i].primitive_count;
    return n;
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
inline void convert_to_narity_node(const bvh::Bvh<float>& original, const bvh::Bvh<float>::Node& node, NArityBvh<N>& bvh, uint32 cur_id)
{
    constexpr size_t MaxIter = 1 << (uint32_log2(N) - 1);

    if (!node.is_leaf()) {
        std::queue<bvh::Bvh<float>::Node> queue;
        queue.push(original.nodes[node.first_child_or_primitive + 0]);
        queue.push(original.nodes[node.first_child_or_primitive + 1]);

        std::vector<bvh::Bvh<float>::Node> children;
        for (size_t k = 0; k < MaxIter && !queue.empty(); ++k) {
            const auto cur = queue.front();
            queue.pop();
            if (cur.is_leaf())
                children.push_back(cur);
            else {
                queue.push(original.nodes[cur.first_child_or_primitive + 0]);
                queue.push(original.nodes[cur.first_child_or_primitive + 1]);
            }
        }

        // Flush out the whole queue
        while (!queue.empty()) {
            children.push_back(queue.front());
            queue.pop();
        }

        // Setup new indices
        bvh.nodes[cur_id].primitive_or_child_count = children.size();
        bvh.nodes[cur_id].first_child_or_primitive = bvh.nodes.size();
        for (auto& child : children)
            bvh.nodes.push_back(clone_node<N>(child));

        // Take care of potential children children
        for (size_t i = 0; i < children.size(); ++i) {
            convert_to_narity_node(original, children[i], bvh, bvh.nodes[cur_id].first_child_or_primitive + i);
        }
    }
}

template <size_t N>
inline void convert_to_narity(const bvh::Bvh<float>& original, NArityBvh<N>& bvh)
{
    bvh.primitive_indices.resize(primitive_count(original));
    bvh.nodes.reserve((original.node_count - 1) / (N / 2));

    bvh.nodes.push_back(clone_node<N>(original.nodes[0]));
    convert_to_narity_node(original, original.nodes[0], bvh, 0);

    for (size_t i = 0; i < bvh.primitive_indices.size(); ++i)
        bvh.primitive_indices[i] = original.primitive_indices[i];
}

template <>
inline void convert_to_narity(const bvh::Bvh<float>& original, NArityBvh<2>& bvh)
{
    // Just copy pasta
    bvh.nodes.resize(original.node_count);
    bvh.primitive_indices.resize(primitive_count(original));

    for (size_t i = 0; i < original.node_count; ++i)
        bvh.nodes[i] = clone_node<2>(original.nodes[i]);

    for (size_t i = 0; i < bvh.primitive_indices.size(); ++i)
        bvh.primitive_indices[i] = original.primitive_indices[i];
}
} // namespace IG