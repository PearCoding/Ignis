#pragma once

#include "IG_Config.h"

namespace IG {
// Special purpose buffer with elements having the same size.
// This table is exposed as a standard DeviceBuffer and elements are not enforced to have the same size.
// Keeping track of the indexes is beyond the scope of this class.
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

        mCount++;
        return mData;
    }

    [[nodiscard]] inline const std::vector<uint8>& data() const { return mData; }
    [[nodiscard]] inline size_t currentOffset() const { return mData.size(); } // TODO: Maybe this should be given as multiple of 4?
    [[nodiscard]] inline size_t entryCount() const { return mCount; }

private:
    size_t mCount = 0;
    std::vector<uint8> mData;
};
} // namespace IG