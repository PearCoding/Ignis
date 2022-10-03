#pragma once

#include "IG_Config.h"

namespace IG {
// Special purpose buffer with elements having the same size
// This table is exposed as a standard DeviceBuffer and no tests are done to ensure all elements having the same size!
class FixTable {
public:
    FixTable() = default;

    inline void reserve(size_t size) { mData.reserve(size); }
    [[nodiscard]] inline std::vector<uint8>& addEntry(size_t alignment)
    {
        if (alignment != 0 && !mData.empty()) {
            size_t defect = alignment - mData.size() % alignment;
            mData.resize(mData.size() + defect);
        }

        if (!mData.empty()) {
            if (mElementSize == 0)
                mElementSize = mData.size();

            IG_ASSERT((mData.size() % mElementSize == 0), "Expected data size be a multiple of the computed element size");
        }

        return mData;
    }

    [[nodiscard]] inline const std::vector<uint8>& data() const { return mData; }
    [[nodiscard]] inline size_t currentOffset() const { return mData.size(); }
    [[nodiscard]] inline size_t elementSize() const { return mElementSize == 0 ? mData.size() : mElementSize; }
    [[nodiscard]] inline size_t entryCount() const { return elementSize() == 0 ? 0 : mData.size() / elementSize(); }

private:
    size_t mElementSize = 0;
    std::vector<uint8> mData;
};
} // namespace IG