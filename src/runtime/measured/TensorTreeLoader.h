#pragma once

#include "IG_Config.h"

namespace IG {
struct TensorTreeComponentSpecification {
    size_t node_count;
    size_t value_count;
    float total;
    bool root_is_leaf;
    float min_proj_sa;
};
struct TensorTreeSpecification {
    size_t ndim;
    TensorTreeComponentSpecification front_reflection;
    TensorTreeComponentSpecification back_reflection;
    TensorTreeComponentSpecification front_transmission;
    TensorTreeComponentSpecification back_transmission;
};

struct TensorTreeNode {
    std::vector<std::unique_ptr<TensorTreeNode>> Children;
    std::vector<float> Values;

    inline bool isLeaf() const { return Children.empty(); }

    // The root is the only node with one child. Get rid of this special case
    inline void eat()
    {
        IG_ASSERT(Values.empty() && Children.size() == 1, "Invalid call to eat");

        std::swap(Children[0]->Values, Values);
        std::swap(Children[0]->Children, Children);
    }

    inline float computeTotal(size_t depth) const
    {
        const float area = 1.0f / (depth * (Values.size() + Children.size()));

        float total = 0;
        for (const auto& child : Children)
            total += child->computeTotal(depth + 1);

        for (float val : Values)
            total += Pi * val * area;

        return total;
    }

    inline size_t computeMaxDepth(size_t depth) const
    {
        if (isLeaf())
            return depth;

        size_t d = 0;
        for (const auto& child : Children)
            d = std::max(d, child->computeMaxDepth(depth + 1));

        return d;
    }

    inline void dump(std::ostream& stream) const
    {
        if (isLeaf()) {
            stream << "[ ";
            for (const auto& c : Values)
                stream << c << " ";
            stream << "]";
        } else {
            stream << "{ ";
            for (const auto& c : Children)
                c->dump(stream);
            stream << " }";
        }
    }
};

class TensorTreeComponent {
public:
    inline explicit TensorTreeComponent(uint32 ndim)
        : mNDim(ndim)
        , mMaxValuesPerNode(1 << ndim)
        , mTotal(0)
        , mMaxDepth(0)
        , mMinProjSA(Pi)
        , mRootIsLeaf(false)
    {
    }

    inline void makeBlack()
    {
        mNodes     = std::vector<NodeValue>(mMaxValuesPerNode, 0);
        mValues    = std::vector<float>(1, static_cast<float>(std::copysign(0, -1)));
        mTotal     = 0;
        mMinProjSA = Pi;

        std::fill(mNodes.begin(), mNodes.end(), -1);
    }

    inline void setRoot(std::unique_ptr<TensorTreeNode>&& node)
    {
        mRoot = std::move(node);
        addNode(*mRoot, std::nullopt);

        mTotal     = mRoot->computeTotal(1);
        mMaxDepth  = mRoot->computeMaxDepth(1);
        mMinProjSA = Pi / (float)(1 << mMaxDepth); // TODO: Validate
    }

    inline void write(std::ostream& os) const
    {
        auto node_count  = static_cast<uint32>(mNodes.size());
        auto value_count = static_cast<uint32>(mValues.size());

        // We do not make use of this header, but it might get handy in other applications
        os.write(reinterpret_cast<const char*>(&mNDim), sizeof(mNDim));
        os.write(reinterpret_cast<const char*>(&mMaxValuesPerNode), sizeof(mMaxValuesPerNode));
        os.write(reinterpret_cast<const char*>(&node_count), sizeof(node_count));
        os.write(reinterpret_cast<const char*>(&value_count), sizeof(value_count));

        os.write(reinterpret_cast<const char*>(mNodes.data()), mNodes.size() * sizeof(NodeValue));
        os.write(reinterpret_cast<const char*>(mValues.data()), mValues.size() * sizeof(float));
    }

    [[nodiscard]] inline size_t nodeCount() const { return mNodes.size(); }
    [[nodiscard]] inline size_t valueCount() const { return mValues.size(); }

    [[nodiscard]] inline float total() const { return mTotal; }
    [[nodiscard]] inline size_t maxDepth() const { return mMaxDepth; }
    [[nodiscard]] inline float isRootLeaf() const { return mRootIsLeaf; }
    [[nodiscard]] inline float minProjSA() const { return mMinProjSA; }

    [[nodiscard]] inline TensorTreeNode* root() { return mRoot.get(); }
    [[nodiscard]] inline const TensorTreeNode* root() const { return mRoot.get(); }

private:
    inline void addNode(const TensorTreeNode& node, std::optional<size_t> parentOffsetValue)
    {
        if (node.isLeaf()) {
            size_t off = mValues.size();
            if (parentOffsetValue.has_value())
                mNodes[parentOffsetValue.value()] = -(static_cast<NodeValue>(off) + 1);
            else
                mRootIsLeaf = true;

            bool single = node.Values.size() == 1;
            if (single) {
                mValues.emplace_back((float)std::copysign(node.Values.front(), -1));
            } else {
                IG_ASSERT(node.Values.size() == mMaxValuesPerNode, "Expected valid number of values in a leaf");
                mValues.insert(mValues.end(), node.Values.begin(), node.Values.end());
            }
        } else {
            IG_ASSERT(node.Children.size() == mMaxValuesPerNode, "Expected valid number of children in a node");

            size_t off = mNodes.size();
            if (parentOffsetValue.has_value())
                mNodes[parentOffsetValue.value()] = static_cast<NodeValue>(off);

            // First create the entries to linearize access
            for (size_t i = 0; i < node.Children.size(); ++i)
                mNodes.emplace_back();

            // Recursively add nodes
            for (size_t i = 0; i < node.Children.size(); ++i)
                addNode(*node.Children[i], off + i);
        }
    }

    using NodeValue = int32_t;
    static_assert(sizeof(NodeValue) == sizeof(float), "Node and Leaf value elements have to have the same size");

    uint32 mNDim;
    uint32 mMaxValuesPerNode;
    std::vector<NodeValue> mNodes;
    std::vector<float> mValues;
    float mTotal;
    size_t mMaxDepth;
    float mMinProjSA;
    bool mRootIsLeaf;

    std::unique_ptr<TensorTreeNode> mRoot;
};

class TensorTree {
public:
    bool is_isotropic;
    std::shared_ptr<TensorTreeComponent> front_reflection;
    std::shared_ptr<TensorTreeComponent> back_reflection;
    std::shared_ptr<TensorTreeComponent> front_transmission;
    std::shared_ptr<TensorTreeComponent> back_transmission;
};

class IG_LIB TensorTreeLoader {
public:
    static bool load(const Path& in_xml, TensorTree& tree);
    static bool prepare(const Path& in_xml, const Path& out_data, TensorTreeSpecification& spec);
};
} // namespace IG