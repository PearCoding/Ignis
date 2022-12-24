#include "VectorSerializer.h"

namespace IG {
VectorSerializer::VectorSerializer(std::vector<uint8>& data, bool readmode)
    : Serializer(readmode)
    , mData(data)
    , mIt(data.size())
{
}

void VectorSerializer::ensureAlignment(size_t alignment)
{
    if (alignment == 0)
        return;

    const size_t defect = alignment - mData.size() % alignment;
    for (size_t i = 0; i < defect; ++i)
        write((uint8)0);
}

bool VectorSerializer::isValid() const
{
    return isReadMode() ? mIt < mData.size() : mIt == mData.size();
}

size_t VectorSerializer::currentSize() const
{
    return mData.size();
}

size_t VectorSerializer::writeRaw(const uint8* data, size_t size)
{
    IG_ASSERT(isValid(), "Trying to write into an invalid serializer!");
    IG_ASSERT(!isReadMode(), "Trying to write into a read serializer!");

    mData.resize(mData.size() + size);
    std::memcpy(&mData[mIt], data, size);

    mIt += size;
    return size;
}

size_t VectorSerializer::readRaw(uint8* data, size_t size)
{
    IG_ASSERT(isValid(), "Trying to read from an invalid serializer!");
    IG_ASSERT(isReadMode(), "Trying to read from a write serializer!");

    const size_t end = std::min(mData.size(), mIt + size);
    const size_t rem = end - mIt;

    std::memcpy(&data[0], &mData[mIt], rem);
    mIt += rem;

    return rem;
}

} // namespace IG