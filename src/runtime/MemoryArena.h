#pragma once

#include "IG_Config.h"

#include <memory>

namespace IG {
/// @brief Basic memory arena which bookkeeps multiple pages of memory, but disallows freeing single units
class MemoryArena {
private:
    const size_t mPageSize;
    size_t mCurrentCapacity;
    size_t mPrevCapacity;
    std::vector<uint8*> mPages;
    size_t mPosInLastPage;

public:
    inline explicit MemoryArena(size_t pageSize = 4096 /* 4kB */)
        : mPageSize(pageSize)
        , mCurrentCapacity(0)
        , mPrevCapacity(0)
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

    [[nodiscard]] inline size_t currentCapacity() const { return mCurrentCapacity; }
    [[nodiscard]] inline size_t currentUsage() const { return mPrevCapacity + mPosInLastPage; }

    [[nodiscard]] inline void* allocateRaw(size_t size)
    {
        if (size == 0)
            return nullptr;

        if (mPages.empty() || mPosInLastPage + size > mPageSize)
            allocatePage(size);

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
        for (auto* ptr : mPages)
            delete[] ptr;
        mPages.clear();
        mCurrentCapacity = 0;
    }

private:
    inline void allocatePage(size_t nextSize)
    {
        mPosInLastPage = 0;

        size_t nextPageSize = mPageSize;
        if (nextSize > mPageSize) {
            // Allocate next page fit for the given size
            const size_t n = nextSize / mPageSize + ((nextSize % mPageSize) > 0 ? 1 : 0);
            nextPageSize *= n;
        }

        mPrevCapacity = mCurrentCapacity;
        mCurrentCapacity += nextPageSize;
        mPages.push_back(new uint8[nextPageSize]);

        IG_ASSERT(mPages.back() != nullptr, "Catched 'Out of Memory' inside assert");
    }
};
} // namespace IG