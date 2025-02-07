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
    static void fillHostWithZero(int dev, void* ptr, size_t sizeInBytes);
    static void copyToHost(int dev, const void* devPtr, void* hostPtr, size_t sizeInBytes);
    static void copyFromHost(int dev, void* devPtr, const void* hostPtr, size_t sizeInBytes);
};

template <typename T>
class UnifiedArray : public UnifiedArrayBase {
    IG_CLASS_NON_COPYABLE(UnifiedArray);

public:
    enum class MemoryType {
        Unified, // Visible on both host & device
        Device,  // Device only
        Shared,  // Device & Host have their own copies and can be synced if necessary
        External // The device is host! So internal pointers are forwarded to prevent useless copies
    };

    enum class Flags : int32 {
        DirtyDevice = 0x1, // The device buffer is dirty and should be mapped to the host (only for shared arrays)
        DirtyHost   = 0x2, // The host buffer is dirty and should be mapped to the device (only for shared arrays)
    };

    T* DevicePtr;
    T* HostPtr;
    size_t SizeInBytes;
    int Device;
    MemoryType Type;
    int32 StatusFlags;

    inline UnifiedArray()
        : DevicePtr(nullptr)
        , HostPtr(nullptr)
        , SizeInBytes(0)
        , Device(0)
        , Type(MemoryType::External)
        , StatusFlags(0)
    {
    }

    inline UnifiedArray(UnifiedArray&& arr)
        : DevicePtr(arr.DevicePtr)
        , HostPtr(arr.HostPtr)
        , SizeInBytes(arr.SizeInBytes)
        , Device(arr.Device)
        , Type(arr.Type)
        , StatusFlags(arr.StatusFlags)
    {
        arr.DevicePtr   = nullptr;
        arr.HostPtr     = nullptr;
        arr.SizeInBytes = 0;
        arr.Device      = 0;
        arr.Type        = MemoryType::External;
        arr.StatusFlags = 0;
    }

    static inline UnifiedArray AllocateUnified(int dev, size_t size)
    {
        void* ptr = allocateUnified(dev, size * sizeof(T));
        return UnifiedArray(
            (T*)ptr,
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

    static inline UnifiedArray AllocateShared(int dev, size_t size)
    {
        void* devPtr = allocateDevice(dev, size * sizeof(T));
        return UnifiedArray(
            dev != 0 ? (T*)allocateDevice(0 /*Host*/, size * sizeof(T)) : (T*)devPtr, // Only allocate buffer if the device != host
            (T*)devPtr,
            size * sizeof(T),
            dev,
            MemoryType::Shared);
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

    static inline UnifiedArray CreateExternalOrAllocateDevice(int dev, T* ptr, size_t size, bool externalFlag)
    {
        if (externalFlag) {
            return CreateExternal(dev, ptr, size);
        } else {
            auto arr = AllocateDevice(dev, size);
            arr.copyFromHost(arr.Device, arr.DevicePtr, ptr, arr.SizeInBytes);
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
        StatusFlags = arr.StatusFlags;

        arr.DevicePtr   = nullptr;
        arr.HostPtr     = nullptr;
        arr.SizeInBytes = 0;
        arr.Device      = 0;
        arr.Type        = MemoryType::External;
        arr.StatusFlags = 0;

        return *this;
    }

    /// @brief Returns true if the device is the host (aka CPU device). This is used for some efficient special cases
    /// @return True if device is host at the same time.
    [[nodiscard]] inline bool isDeviceHost() const { return Device == 0; }

    inline void fillHostWithZero()
    {
        UnifiedArrayBase::fillHostWithZero(Device, (void*)HostPtr, SizeInBytes);
    }

    inline void copyFromExternalHostToDevice(const T* hostPtr)
    {
        UnifiedArrayBase::copyFromHost(Device, DevicePtr, hostPtr, SizeInBytes);
        StatusFlags |= (int)Flags::DirtyDevice;
    }

    inline void copyFromDeviceToExternalHost(T* hostPtr)
    {
        UnifiedArrayBase::copyToHost(Device, DevicePtr, hostPtr, SizeInBytes);
    }

    inline void markDirtyOnDevice()
    {
        IG_ASSERT((StatusFlags & (int)Flags::DirtyHost) == 0, "Can not mark the device dirty when host is already dirty");
        StatusFlags |= (int)Flags::DirtyDevice;
    }

    inline void markDirtyOnHost()
    {
        IG_ASSERT((StatusFlags & (int)Flags::DirtyDevice) == 0, "Can not mark the host dirty when device is already dirty");
        StatusFlags |= (int)Flags::DirtyHost;
    }

    /// @brief If the array is shared, will make sure both buffers are in sync
    inline void sync()
    {
        syncForDevice();
        syncForHost();
    }

    /// @brief If the array is shared, will make sure the device buffer is in sync with the host
    inline void syncForDevice()
    {
        if (Type != MemoryType::Shared || HostPtr == DevicePtr)
            return;

        IG_ASSERT((StatusFlags & ((int)Flags::DirtyDevice | (int)Flags::DirtyHost)) != ((int)Flags::DirtyDevice | (int)Flags::DirtyHost), "Unified array can not deal with device and host buffer being out of sync at the same time!");

        if (StatusFlags & (int)Flags::DirtyHost)
            UnifiedArrayBase::copyFromHost(Device, DevicePtr, HostPtr, SizeInBytes);

        StatusFlags &= ~(int)Flags::DirtyHost;
    }

    /// @brief If the array is shared, will make sure the device buffer is in sync with the host
    inline void syncForHost()
    {
        if (Type != MemoryType::Shared || HostPtr == DevicePtr)
            return;

        IG_ASSERT((StatusFlags & ((int)Flags::DirtyDevice | (int)Flags::DirtyHost)) != ((int)Flags::DirtyDevice | (int)Flags::DirtyHost), "Unified array can not deal with device and host buffer being out of sync at the same time!");

        if (StatusFlags & (int)Flags::DirtyDevice)
            UnifiedArrayBase::copyToHost(Device, DevicePtr, HostPtr, SizeInBytes);

        StatusFlags &= ~(int)Flags::DirtyDevice;
    }

private:
    inline UnifiedArray(T* hostPtr, T* devicePtr, size_t sizeInBytes, int device, MemoryType type)
        : DevicePtr(devicePtr)
        , HostPtr(hostPtr)
        , SizeInBytes(sizeInBytes)
        , Device(device)
        , Type(type)
        , StatusFlags(0)
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
        case MemoryType::Shared:
            if (HostPtr != DevicePtr) {
                deallocateDevice(0 /*Host*/, (void*)HostPtr);
                deallocateDevice(Device, (void*)DevicePtr);
            } else {
                deallocateDevice(Device, (void*)DevicePtr);
            }
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
        StatusFlags = 0;
    }
};
} // namespace IG