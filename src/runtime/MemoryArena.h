#pragma once

#include "IG_Config.h"

#include <memory>

namespace IG {
/// @brief Basic memory arena which bookkeeps multiple pages of memory, but disallows freeing single units
class MemoryArena {
private:
    const size_t mPageSize;
    std::vector<uint8*> mPages;
    size_t mPosInLastPage;

public:
    inline explicit MemoryArena(size_t pageSize = 4096 /* 4kB */)
        : mPageSize(pageSize)
        , mPages()
        , mPosInLastPage(0)
    {
    }

    inline ~MemoryArena()
    {
        release();
    }

    // Allow move
    MemoryArena(MemoryArena&& other)            = default;
    MemoryArena& operator=(MemoryArena&& other) = default;

    // No copy
    MemoryArena(const MemoryArena& other)            = delete;
    MemoryArena& operator=(const MemoryArena& other) = delete;

    [[nodiscard]] inline size_t currentCapacity() const { return mPages.size() * mPageSize; }
    [[nodiscard]] inline size_t currentUsage() const { return mPages.empty() ? 0 : ((mPages.size() - 1) * mPageSize + mPosInLastPage); }

    [[nodiscard]] inline void* allocateRaw(size_t size)
    {
        if (size == 0)
            return nullptr;

        IG_ASSERT(size <= mPageSize, "Expected page size to be greater or equal to the largest allocation");

        if (mPages.empty() || mPosInLastPage + size > mPageSize)
            allocatePage();

        const auto ptr = mPages.back() + mPosInLastPage;
        mPosInLastPage += size;
        return ptr;
    }

    template <typename T, class... Args>
    [[nodiscard]] inline T* allocateObject(Args&&... args)
    {
        return std::construct_at((T*)allocateRaw(sizeof(T)), std::forward<Args>(args)...);
    }

    inline void release()
    {
        mPosInLastPage = 0;
        for (void* ptr : mPages)
            delete[] ptr;
        mPages.clear();
    }

private:
    inline void allocatePage()
    {
        mPosInLastPage = 0;
        mPages.push_back(new uint8[mPageSize]);

        IG_ASSERT(mPages.back() != nullptr, "Catched Out of Memory inside assert");
    }
};
} // namespace IG