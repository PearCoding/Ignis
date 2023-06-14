#pragma once

#include "AnyDSLRuntime.h"

/// Some arrays do not need new allocations at the host if the data is already provided and properly arranged.
/// However, this assumes that the pointer is always properly aligned!
/// For external devices (e.g., GPU) a standard anydsl array will be used
// TODO: Make a fallback for cases in which the pointer is not aligned
template <typename T>
class ShallowArray {
public:
    inline ShallowArray()
        : mDeviceMem()
        , mHostMem(nullptr)
        , mDevice()
        , mSize(0)
    {
    }

    inline ShallowArray(const anydsl::Device& dev, const T* ptr, size_t n)
        : mDeviceMem()
        , mHostMem(nullptr)
        , mDevice(dev)
        , mSize(n)
    {
        if (!dev.isHost()) {
            if (n != 0) {
                mDeviceMem = std::move(anydsl::Array<T>(dev, n));
                IG_CHECK_ANYDSL(anydslCopyBufferFromHost(mDeviceMem.handle(), 0, sizeof(T) * n, ptr));
            }
        } else {
            mHostMem = ptr;
        }
    }

    inline ShallowArray(ShallowArray&&)            = default;
    inline ShallowArray& operator=(ShallowArray&&) = default;

    inline ~ShallowArray() = default;

    inline const anydsl::Array<T>& deviceData() const { return mDeviceMem; }
    inline const T* hostData() const { return mHostMem; }

    inline const T* ptr() const
    {
        if (isHost())
            return hostData();
        else
            return deviceData().data();
    }

    inline bool hasData() const { return ptr() != nullptr && mSize > 0; }
    inline bool isHost() const { return mHostMem != nullptr && mDevice.isHost(); }
    inline size_t size() const { return mSize; }

private:
    anydsl::Array<T> mDeviceMem;
    const T* mHostMem;
    anydsl::Device mDevice;
    size_t mSize;
};