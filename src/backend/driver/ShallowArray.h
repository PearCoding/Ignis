#pragma once

#include "IG_Config.h"

#include <anydsl_runtime.hpp>

/// Some arrays do not need new allocations at the host if the data is already provided and properly arranged.
/// However, this assumes that the pointer is always properly aligned!
/// For external devices (e.g., GPU) a standard anydsl array will be used
// TODO: Make a fallback for cases in which the pointer is not aligned
template <typename T>
class ShallowArray {
public:
    inline ShallowArray()
        : device_mem()
        , host_mem(nullptr)
        , device(0)
        , size_(0)
    {
    }

    inline ShallowArray(int32_t dev, const T* ptr, size_t n)
        : device_mem()
        , host_mem(nullptr)
        , device(dev)
        , size_(n)
    {
        if (dev != 0) {
            if (n != 0) {
                device_mem = std::move(anydsl::Array<T>(dev, reinterpret_cast<T*>(anydsl_alloc(dev, sizeof(T) * n)), n));
                anydsl_copy(0, ptr, 0, dev, device_mem.data(), 0, sizeof(T) * n);
            }
        } else {
            host_mem = ptr;
        }
    }

    inline ShallowArray(ShallowArray&&) = default;
    inline ShallowArray& operator=(ShallowArray&&) = default;

    inline ~ShallowArray() = default;

    inline const anydsl::Array<T>& device_data() const { return device_mem; }
    inline const T* host_data() const { return host_mem; }

    inline const T* ptr() const
    {
        if (is_host())
            return host_data();
        else
            return device_data().data();
    }

    inline bool has_data() const { return ptr() != nullptr && size_ > 0; }
    inline bool is_host() const { return host_mem != nullptr && device == 0; }
    inline size_t size() const { return size_; }

private:
    anydsl::Array<T> device_mem;
    const T* host_mem;
    int32_t device;
    size_t size_;
};