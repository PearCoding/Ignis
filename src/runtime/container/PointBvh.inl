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

    mLeafNodes.emplace_back(elem);

    if (mInnerNodes.empty()) {
        mInnerNodes.emplace_back(InnerNode::makeLeaf(BoundingBox(p)));
    } else {
        InnerNode* node = getForPointExtend(p);

        IG_ASSERT(node && node->isLeaf(), "Expected a valid leaf");
        const size_t leafIdx    = mLeafNodes.size() - 1;
        const size_t oldLeafIdx = node->Index;

        const BoundingBox nodeBox = node->BBox;

        int axis  = 0;
        float mid = nodeBox.diameter().maxCoeff(&axis) / 2;

        const size_t leftIdx = mInnerNodes.size();

        // Make node an inner
        node->Index = leftIdx;
        node->Axis  = axis;
        node->Mid   = mid;
        node->BBox  = nodeBox;

        BoundingBox leftBox;
        BoundingBox rightBox;
        nodeBox.computeSplit(leftBox, rightBox, axis, 0.5f);

        // Construct two new leaf nodes
        mInnerNodes.emplace_back(InnerNode::makeLeaf(leftBox));
        mInnerNodes.emplace_back(InnerNode::makeLeaf(rightBox));

        auto& left  = mInnerNodes[leftIdx];
        auto& right = mInnerNodes[leftIdx + 1];

        if (p[axis] < mid) {
            left.Index  = leafIdx;
            right.Index = oldLeafIdx;
        } else {
            left.Index  = oldLeafIdx;
            right.Index = leafIdx;
        }
    }
}

template <class T, template <typename> typename PositionGetter>
inline typename PointBvh<T, PositionGetter>::InnerNode* PointBvh<T, PositionGetter>::getForPointExtend(const Vector3f& p)
{
    if (mInnerNodes.empty())
        return nullptr;

    return getForPointExtend(&mInnerNodes.at(0), p);
}

template <class T, template <typename> typename PositionGetter>
inline typename PointBvh<T, PositionGetter>::InnerNode* PointBvh<T, PositionGetter>::getForPointExtend(InnerNode* node, const Vector3f& p)
{
    // Add new point to the bbox
    node->BBox.extend(p);

    if (node->isLeaf())
        return node;

    const float mid = node->BBox.center()[node->Axis];
    const float m   = p[node->Axis];
    if (m < mid)
        return getForPointExtend(&mInnerNodes.at(node->Index), p);
    else
        return getForPointExtend(&mInnerNodes.at(node->Index + 1), p);
}
} // namespace IG