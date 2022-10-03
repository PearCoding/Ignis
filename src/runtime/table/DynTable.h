#pragma once

#include "IG_Config.h"

namespace IG {
struct LookupEntry {
    uint32 TypeID;
    uint32 Flags;
    uint64 Offset; // TODO: Explicitly split it to two uint32 for compliance with join_u32_to_u64 in data.art
};

class DynTable {
public:
    DynTable() = default;

    [[nodiscard]] inline size_t entryCount() const { return mLookups.size(); }
    inline void reserve(size_t size) { mData.reserve(size); }
    [[nodiscard]] inline std::vector<uint8>& addLookup(uint32 typeID, uint32 flags, size_t alignment)
    {
        if (alignment != 0 && !mData.empty()) {
            size_t defect = alignment - mData.size() % alignment;
            mData.resize(mData.size() + defect);
        }

        mLookups.push_back(LookupEntry{ typeID, flags, (uint64)mData.size() });
        return mData;
    }

    [[nodiscard]] inline const std::vector<LookupEntry>& lookups() const { return mLookups; }
    [[nodiscard]] inline const std::vector<uint8>& data() const { return mData; }
    [[nodiscard]] inline size_t currentOffset() const { return mData.size(); } // TODO: Maybe this should be given as multiple of 4?

private:
    std::vector<LookupEntry> mLookups;
    std::vector<uint8> mData;
};
} // namespace IG