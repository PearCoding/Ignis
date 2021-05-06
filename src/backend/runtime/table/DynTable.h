#pragma once

#include "IG_Config.h"

namespace IG {
struct LookupEntry {
	uint32 TypeID;
	uint32 Flags;
	uint64 Offset;
};

class DynTable {
public:
	DynTable() = default;

	inline size_t entryCount() const { return mLookups.size(); }
	inline void reserve(size_t size) { mData.reserve(size); }
	inline std::vector<uint8>& addLookup(uint32 typeID, size_t alignment = 0)
	{
		if (alignment != 0 && !mData.empty()) {
			size_t defect = alignment - mData.size() % alignment;
			mData.resize(mData.size() + defect);
		}
		
		mLookups.push_back(LookupEntry{ typeID, 0, (uint64)mData.size() });
		return mData;
	}

	inline const std::vector<LookupEntry>& lookups() const { return mLookups; }
	inline const std::vector<uint8>& data() const { return mData; }

private:
	std::vector<LookupEntry> mLookups;
	std::vector<uint8> mData;
};
} // namespace IG