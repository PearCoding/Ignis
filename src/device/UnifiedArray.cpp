#include "UnifiedArray.h"

#include <anydsl_runtime.h>

namespace IG {
void* UnifiedArrayBase::allocateUnified(int dev, size_t sizeInBytes)
{
    return anydsl_alloc_host(dev, (int64_t)sizeInBytes);
}

void* UnifiedArrayBase::allocateDevice(int dev, size_t sizeInBytes)
{
    return anydsl_alloc(dev, (int64_t)sizeInBytes);
}

void UnifiedArrayBase::deallocateUnified(int dev, void* ptr)
{
    if (ptr)
        anydsl_release_host(dev, ptr);
}

void UnifiedArrayBase::deallocateDevice(int dev, void* ptr)
{
    if (ptr)
        anydsl_release(dev, ptr);
}

void* UnifiedArrayBase::getDevicePtr(int dev, void* hostPtr)
{
    if (dev == 0)
        return hostPtr;

    if (hostPtr)
        return anydsl_get_device_ptr(dev, hostPtr);
    return nullptr;
}

void UnifiedArrayBase::fillWithZero(int dev, void* ptr, size_t sizeInBytes)
{
    IG_UNUSED(dev);
    std::memset(ptr, 0, sizeInBytes);
}

} // namespace IG