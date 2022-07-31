namespace IG {
template <class T, template <typename> typename PositionGetter>
PointBvh<T, PositionGetter>::PointBvh(const PositionGetter<T>& getter)
    : mInnerNodes()
    , mLeafNodes()
    , mPositionGetter(getter)
{
}

template <class T, template <typename> typename PositionGetter>
PointBvh<T, PositionGetter>::~PointBvh()
{
}

template <class T, template <typename> typename PositionGetter>
void PointBvh<T, PositionGetter>::reset()
{
    mInnerNodes.clear();
    mLeafNodes.clear();
}

template <class T, template <typename> typename PositionGetter>
void PointBvh<T, PositionGetter>::store(const T& elem)
{
    const auto p = mPositionGetter(elem);

    mBox.extend(p);
    mLeafNodes.push_back(elem);

    if (mInnerNodes.empty()) {
        mInnerNodes.emplace_back(InnerNode::makeLeaf(BoundingBox(p)));
    } else {
        InnerNode* node = getForPoint(p);

        IG_ASSERT(node && node->isLeaf(), "Expected a valid leaf");
        const size_t leafIdx    = mLeafNodes.size() - 1;
        const size_t oldLeafIdx = node->Index;

        const auto otherP         = mPositionGetter(mLeafNodes.at(node->Index));
        const BoundingBox nodeBox = BoundingBox(otherP).extend(p);

        int axis  = 0;
        float mid = nodeBox.diameter().maxCoeff(&axis) / 2;

        const size_t leftIdx = mInnerNodes.size();

        // Make node an inner
        node->Index = leftIdx;
        node->Axis  = axis;
        node->BBox  = nodeBox;

        BoundingBox leftBox;
        BoundingBox rightBox;
        nodeBox.computeSplit(leftBox, rightBox, axis, 0.5f);

        // Construct two new leaf nodes
        auto& left  = mInnerNodes.emplace_back(InnerNode::makeLeaf(leftBox));
        auto& right = mInnerNodes.emplace_back(InnerNode::makeLeaf(rightBox));

        if (p[axis] <= mid) {
            left.Index  = leafIdx;
            right.Index = oldLeafIdx;
        } else {
            left.Index  = oldLeafIdx;
            right.Index = leafIdx;
        }
    }
}

template <class T, template <typename> typename PositionGetter>
inline typename PointBvh<T, PositionGetter>::InnerNode* PointBvh<T, PositionGetter>::getForPoint(const Vector3f& p)
{
    if (mInnerNodes.empty())
        return nullptr;

    if (!mBox.isInside(p))
        return nullptr;

    return getForPoint(&mInnerNodes.at(0), p);
}

template <class T, template <typename> typename PositionGetter>
inline typename PointBvh<T, PositionGetter>::InnerNode* PointBvh<T, PositionGetter>::getForPoint(InnerNode* node, const Vector3f& p)
{
    if (node->isLeaf())
        return node;

    const float m = p[node->Axis];
    if (m <= node->BBox.center()[node->Axis])
        return getForPoint(&mInnerNodes.at(node->Index), p);
    else
        return getForPoint(&mInnerNodes.at(node->Index + 1), p);
}
} // namespace IG