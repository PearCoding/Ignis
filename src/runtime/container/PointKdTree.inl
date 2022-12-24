namespace IG {
template <class T, template <typename> typename PositionGetter>
PointKdTree<T, PositionGetter>::PointKdTree(size_t max_points, const PositionGetter<T>& getter)
    : mElements(max_points + 1)
    , mStoredElements(0)
    , mMaxElements(max_points)
    , mPositionGetter(getter)
{
}

template <class T, template <typename> typename PositionGetter>
PointKdTree<T, PositionGetter>::~PointKdTree()
{
}

template <class T, template <typename> typename PositionGetter>
void PointKdTree<T, PositionGetter>::reset()
{
    mStoredElements = 0;
}

template <class T, template <typename> typename PositionGetter>
void PointKdTree<T, PositionGetter>::store(const T& pht)
{
    if (isFull())
        return;

    mStoredElements++;
    mElements[mStoredElements] = ElementWrapper{ pht, 0 };

    mBox.combine(mPositionGetter(pht));
}

template <class T, template <typename> typename PositionGetter>
template <typename Function>
void PointKdTree<T, PositionGetter>::traverse(Function func) const
{
    for (const auto& p : mElements)
        func(p.Element);
}

template <class T, template <typename> typename PositionGetter>
void PointKdTree<T, PositionGetter>::balanceTree()
{
    // Pointers
    ElementWrapper** pTmp1 = new ElementWrapper*[mStoredElements + 1];
    ElementWrapper** pTmp2 = new ElementWrapper*[mStoredElements + 1];

    IG_ASSERT(pTmp1 && pTmp2, "Not enough memory while balancing the tree");

    for (uint64 i = 0; i <= mStoredElements; ++i)
        pTmp2[i] = &mElements[i];

    balanceSegment(pTmp1, pTmp2, 1, 1, mStoredElements);
    delete[] pTmp2;
    pTmp2 = nullptr; // For debug!

    // Now reorganize balanced kd-tree to make use of the heap design
    uint64 d, j = 1, h = 1;
    ElementWrapper h_photon = mElements[j];

    for (uint64 i = 1; i <= mStoredElements; ++i) {
        d        = (uint64)(pTmp1[j] - &mElements[0]) /* sizeof(ElementWrapper*)*/;
        pTmp1[j] = nullptr;

        if (d != h) {
            mElements[j] = mElements[d];
            j            = d;
        } else {
            mElements[j] = h_photon;

            // Not the last entry
            if (i < mStoredElements) {
                for (; h <= mStoredElements; ++h) {
                    if (pTmp1[h])
                        break;
                }

                h_photon = mElements[h];
                j        = h;
            }
        }
    }

    delete[] pTmp1;
}

template <class T, template <typename> typename PositionGetter>
void PointKdTree<T, PositionGetter>::balanceSegment(ElementWrapper** balance, ElementWrapper** original,
                                                    uint64 index, uint64 start, uint64 end)
{
    // Calculate median
    uint64 median = 1;
    while ((4 * median) <= (end - start + 1))
        median += median;

    if ((3 * median) <= (end - start + 1))
        median += median + start - 1;
    else
        median = end - median + 1;

    // Find axis
    int axis = 2; // Z axis
    if (mBox.width() > mBox.height() && mBox.width() > mBox.depth())
        axis = 0; // X axis
    else if (mBox.height() > mBox.depth())
        axis = 1; // Y axis

    // Partition
    medianSplit(original, start, end, median, axis);
    balance[index]       = original[median];
    balance[index]->Axis = axis;

    // Recursively do the left and right part
    if (median > start) {
        if (start < median - 1) {
            const float tmp         = mBox.upperBound()(axis);
            mBox.upperBound()(axis) = mPositionGetter(balance[index]->Element)[axis];
            balanceSegment(balance, original, 2 * index, start, median - 1);
            mBox.upperBound()(axis) = tmp;
        } else {
            balance[2 * index] = original[start];
        }
    }

    if (median < end) {
        if (median + 1 < end) {
            const float tmp         = mBox.lowerBound()(axis);
            mBox.lowerBound()(axis) = mPositionGetter(balance[index]->Element)[axis];
            balanceSegment(balance, original, 2 * index + 1, median + 1, end);
            mBox.lowerBound()(axis) = tmp;
        } else {
            balance[2 * index + 1] = original[end];
        }
    }
}

template <class T, template <typename> typename PositionGetter>
void PointKdTree<T, PositionGetter>::medianSplit(ElementWrapper** elements, uint64 start, uint64 end, uint64 median, int axis)
{
    uint64 left  = start;
    uint64 right = end;

    while (right > left) {
        const float v = mPositionGetter(elements[right]->Element)[axis];
        uint64 i      = left - 1; // FIXME: A underflow is happening here
        uint64 j      = right;
        while (true) {
            while (mPositionGetter(elements[++i]->Element)[axis] < v)
                ;
            while (mPositionGetter(elements[--j]->Element)[axis] > v && j > left)
                ;

            if (i >= j)
                break;

            std::swap(elements[i], elements[j]);
        }
        std::swap(elements[i], elements[right]);

        if (i >= median)
            right = i - 1;
        if (i <= median)
            left = i + 1;
    }
}
} // namespace IG