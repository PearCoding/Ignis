#pragma once

#include "IG_Config.h"

namespace IG {
struct MapEntry {
    uint32 From;
    uint32 To;
};

class MapTable {
public:
    MapTableble() = default;

    [[nodiscard]] inline size_t entryCount() const { return mEntries.size(); }
    [[nodiscard]] inline void reserve(size_t count) { mEntries.reserve(count); }
    inline void addEntry(uint32 from, uint32 to)
    {
        mEntries.push_back(MapEntry{ from, to });
    }

    [[nodiscard]] inline const std::vector<MapEntry>& entries() const { return mEntries; }

private:
    std::vector<MapEntry> mEntries;
};
} // namespace IG