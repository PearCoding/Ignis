#pragma once

#include "PositionGetter.h"
#include "math/BoundingBox.h"

namespace IG {
// Based on the implementation in the book:
// Realistic Image Synthesis Using Photon Mapping (2nd Edition: 2001)
// from Henrik Wann Jensen
template <class T, template <typename> typename PositionGetter = position_getter>
class PointKdTree {
    IG_CLASS_NON_COPYABLE(PointKdTree);

public:
    template <typename U = T>
    PointKdTree(size_t max_points,
                typename std::enable_if<std::is_default_constructible<PositionGetter<U>>::value>::type* = 0)
        : PointKdTree(max_points, PositionGetter<U>())
    {
    }
    inline PointKdTree(size_t max_points, const PositionGetter<T>& getter);
    virtual ~PointKdTree();

    inline void reset();

    [[nodiscard]] inline bool isFull() const { return mStoredElements >= mMaxElements; }
    [[nodiscard]] inline bool isEmpty() const { return mStoredElements == 0; }
    [[nodiscard]] inline uint64 storedElements() const { return mStoredElements; }

    inline void store(const T& point);

    template <typename Function>
    inline void traverse(Function func) const;

    inline void balanceTree(); // Balance the KD-tree before using

private:
    struct ElementWrapper {
        T Element;
        uint8 Axis = 0;
    };

    // KD-tree utils
    inline void balanceSegment(ElementWrapper** balance, ElementWrapper** original,
                               uint64 index, uint64 start, uint64 end);
    inline void medianSplit(ElementWrapper** elements, uint64 start, uint64 end, uint64 median, int axis);

    std::vector<ElementWrapper> mElements;
    size_t mStoredElements;
    const size_t mMaxElements;

    BoundingBox mBox;

    const PositionGetter<T> mPositionGetter;
};
} // namespace IG

#include "PointKdTree.inl"