#pragma once

#include "PositionGetter.h"
#include "math/BoundingBox.h"

namespace IG {
/// Basic BVH implementation without SAH
/// This is intended to be used for points
/// Arity is fixed to 2
template <class T, template <typename> typename PositionGetter = DefaultPositionGetter>
class PointBvh {
    IG_CLASS_NON_COPYABLE(PointBvh);

public:
    struct InnerNode {
        size_t Index;
        BoundingBox BBox;
        int Axis;

        [[nodiscard]] inline size_t leftIndex() const { return Index; }
        [[nodiscard]] inline size_t rightIndex() const { return Index + 1; }
        [[nodiscard]] inline bool isLeaf() const { return Axis < 0; }
        [[nodiscard]] static inline InnerNode makeLeaf(const BoundingBox& bbox) { return InnerNode{ 0, bbox, -1 }; }
    };

    template <typename U = T>
    PointBvh(typename std::enable_if<std::is_default_constructible<PositionGetter<U>>::value>::type* = 0)
        : PointBvh(PositionGetter<U>())
    {
    }
    inline explicit PointBvh(const PositionGetter<T>& getter);
    virtual ~PointBvh();

    inline void reset();

    [[nodiscard]] inline bool isEmpty() const { return mLeafNodes.empty(); }
    [[nodiscard]] inline size_t storedElements() const { return mLeafNodes.size(); }

    inline void store(const T& point);

    [[nodiscard]] inline const std::vector<InnerNode>& innerNodes() const { return mInnerNodes; }
    [[nodiscard]] inline const std::vector<T>& leafNodes() const { return mLeafNodes; }

private:
    inline InnerNode* getForPoint(const Vector3f& p);
    inline InnerNode* getForPoint(InnerNode* node, const Vector3f& p);

    std::vector<InnerNode> mInnerNodes;
    std::vector<T> mLeafNodes;

    BoundingBox mBox;

    const PositionGetter<T> mPositionGetter;
};
} // namespace IG

#include "PointBvh.inl"