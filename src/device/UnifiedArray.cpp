#include "UnifiedArray.h"

#include <anydsl_runtime.h>

namespace IG {
void* UnifiedArrayBase::allocateUnified(int dev, size_t sizeInBytes)
{
    return anydsl_alloc_unified(dev, (int64_t)sizeInBytes);
}

void* UnifiedArrayBase::allocateDevice(int dev, size_t sizeInBytes)
{
    return anydsl_alloc(dev, (int64_t)sizeInBytes);
}

void UnifiedArrayBase::deallocateUnified(int dev, void* ptr)
{
    if (ptr)
        anydsl_release(dev, ptr);
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

void UnifiedArrayBase::copyToHost(int dev, const void* devPtr, void* hostPtr, size_t sizeInBytes)
{
    if (dev == 0 /*Host*/ && devPtr == hostPtr)
        return;

    anydsl_copy(dev, devPtr, 0, 0, hostPtr, 0, (int64)sizeInBytes);
}

void UnifiedArrayBase::copyFromHost(int dev, void* devPtr, const void* hostPtr, size_t sizeInBytes)
{
    if (dev == 0 /*Host*/ && devPtr == hostPtr)
        return;

    anydsl_copy(0, hostPtr, 0, dev, devPtr, 0, (int64)sizeInBytes);
}

} // namespace IG