#pragma once

#include "IG_Config.h"

namespace IG {
class UnifiedArrayBase {
protected:
    [[nodiscard]] static void* allocateUnified(int dev, size_t sizeInBytes);
    [[nodiscard]] static void* allocateDevice(int dev, size_t sizeInBytes);
    static void deallocateUnified(int dev, void* ptr);
    static void deallocateDevice(int dev, void* ptr);
    [[nodiscard]] static void* getDevicePtr(int dev, void* hostPtr);
    static void fillWithZero(int dev, void* ptr, size_t sizeInBytes);
};

template <typename T>
class UnifiedArray : public UnifiedArrayBase {
    IG_CLASS_NON_COPYABLE(UnifiedArray);

public:
    enum class MemoryType {
        Unified, // Visible on both host & device
        Device,  // Device only
        External // The device is host! So internal pointers are forwarded to prevent useless copies
    };

    T* DevicePtr;
    T* HostPtr;
    size_t SizeInBytes;
    int Device;
    MemoryType Type;

    inline UnifiedArray()
        : DevicePtr(nullptr)
        , HostPtr(nullptr)
        , SizeInBytes(0)
        , Device(0)
        , Type(MemoryType::External)
    {
    }

    inline UnifiedArray(UnifiedArray&& arr)
        : DevicePtr(arr.DevicePtr)
        , HostPtr(arr.HostPtr)
        , SizeInBytes(arr.SizeInBytes)
        , Device(arr.Device)
        , Type(arr.Type)
    {
        arr.DevicePtr   = nullptr;
        arr.HostPtr     = nullptr;
        arr.SizeInBytes = 0;
        arr.Device      = 0;
        arr.Type        = MemoryType::External;
    }

    static inline UnifiedArray AllocateUnified(int dev, size_t size)
    {
        void* ptr = allocateUnified(dev, size * sizeof(T));
        return UnifiedArray(
            (T*)ptr,
            // (T*)getDevicePtr(dev, ptr),
            (T*)ptr,
            size * sizeof(T),
            dev,
            MemoryType::Unified);
    }

    static inline UnifiedArray AllocateDevice(int dev, size_t size)
    {
        return UnifiedArray(
            nullptr,
            (T*)allocateDevice(dev, size * sizeof(T)),
            size * sizeof(T),
            dev,
            MemoryType::Device);
    }

    static inline UnifiedArray CreateExternal(int dev, T* ptr, size_t size)
    {
        return UnifiedArray(
            ptr,
            ptr,
            size * sizeof(T),
            dev,
            MemoryType::External);
    }

    static inline UnifiedArray CreateExternalOrAllocateUnified(int dev, T* ptr, size_t size, bool externalFlag)
    {
        if (externalFlag) {
            return CreateExternal(dev, ptr, size);
        } else {
            auto arr = AllocateUnified(dev, size);
            std::memcpy(arr.HostPtr, ptr, size * sizeof(T));
            return arr;
        }
    }

    inline ~UnifiedArray()
    {
        release();
    }

    inline UnifiedArray& operator=(UnifiedArray&& arr)
    {
        release();

        DevicePtr   = arr.DevicePtr;
        HostPtr     = arr.HostPtr;
        SizeInBytes = arr.SizeInBytes;
        Device      = arr.Device;
        Type        = arr.Type;

        arr.DevicePtr   = nullptr;
        arr.HostPtr     = nullptr;
        arr.SizeInBytes = 0;
        arr.Device      = 0;
        arr.Type        = MemoryType::External;

        return *this;
    }

    inline void fillWithZero()
    {
        UnifiedArrayBase::fillWithZero(Device, (void*)HostPtr, SizeInBytes);
    }

private:
    inline UnifiedArray(T* hostPtr, T* devicePtr, size_t sizeInBytes, int device, MemoryType type)
        : DevicePtr(devicePtr)
        , HostPtr(hostPtr)
        , SizeInBytes(sizeInBytes)
        , Device(device)
        , Type(type)
    {
    }

    inline void release()
    {
        switch (Type) {
        case MemoryType::Unified:
            deallocateUnified(Device, (void*)HostPtr);
            break;
        case MemoryType::Device:
            deallocateDevice(Device, (void*)DevicePtr);
            break;
        default:
        case MemoryType::External:
            break;
        }

        HostPtr     = nullptr;
        DevicePtr   = nullptr;
        SizeInBytes = 0;
        Device      = 0;
        Type        = MemoryType::External;
    }
};
} // namespace IG