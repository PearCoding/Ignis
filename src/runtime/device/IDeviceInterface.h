#pragma once

#include "Device.h"

namespace IG {
struct LookupEntry;
class IG_LIB IDeviceInterface {
public:
    template <typename T>
    struct DeviceBufferProxy {
        T* DataPtr;
        size_t SizeInBytes;
        inline static DeviceBufferProxy Invalid() { return { .DataPtr = nullptr, .SizeInBytes = 0 }; }
    };
    using FixTableProxy = DeviceBufferProxy<uint8_t>;

    template <typename T>
    struct DeviceImageProxy {
        T* DataPtr;
        size_t Width;
        size_t Height;
        inline static DeviceImageProxy Invalid() { return { .DataPtr = nullptr, .Width = 0, .Height = 0 }; }
    };

    template <typename T>
    struct DeviceStreamProxy {
        T* DataPtr;
        size_t BlockSize;
        inline static DeviceStreamProxy Invalid() { return { .DataPtr = nullptr, .BlockSize = 0 }; }
    };

    struct DynTableProxy {
        size_t EntryCount;
        const LookupEntry* LookupEntries;
        uint8_t* DataPtr;
        size_t DataSize;
    };

    struct TemporaryStorageHostProxy {
        int32_t* RayBeginsPtr;
        int32_t* RayEndsPtr;
    };

    enum class StreamType : int {
        Primary = 0,
        Secondary,
        Count
    };

    enum class TraversalStage : int {
        Primary,
        Secondary
    };

    enum class BVHType : int {
        BVH2,
        BVH4,
        BVH8
    };

    virtual ~IDeviceInterface() = default;

    virtual bool isGPU() const    = 0;
    virtual Target target() const = 0;
    virtual int deviceID() const  = 0;

    virtual void releaseAllMemory() = 0;

    virtual void connectGlobalRegistry(ParameterSet* parameter_set)     = 0;
    virtual void setCurrentSceneSettings(const Device::SceneSettings&)  = 0;
    virtual const Device::SceneSettings& currentSceneSettings() const   = 0;
    virtual const Device::RenderSettings& currentRenderSettings() const = 0;

    virtual std::pair<size_t, size_t> framebufferSize() const = 0;
    virtual std::pair<size_t, size_t> workSize() const        = 0;

    inline size_t framebufferArea() const { return std::get<0>(framebufferSize()) * std::get<1>(framebufferSize()); }
    inline size_t workArea() const { return std::get<0>(workSize()) * std::get<1>(workSize()); }

    virtual std::vector<std::string> getAOVNames() const        = 0;
    virtual DeviceImageProxy<float> getFramebufferForDevice()   = 0;
    virtual void resizeFramebuffer(size_t width, size_t height) = 0;

    virtual std::string lookupResource(int32_t id) const = 0;

    virtual void registerThread()   = 0;
    virtual void unregisterThread() = 0;

    virtual ParameterSet* getCurrentGlobalRegistry() = 0;
    virtual ParameterSet* getCurrentLocalRegistry()  = 0;

    virtual DeviceStreamProxy<float> getStream(StreamType type, size_t buffer_id, size_t size, size_t minComponents) = 0;
    virtual DeviceStreamProxy<float> getStream(StreamType type, size_t buffer_id)                                    = 0;
    virtual void swapGPUStreams(StreamType type)                                                                     = 0;

    virtual TemporaryStorageHostProxy getTemporaryStorageHost() = 0; // TODO: Would be cool to get rid of this and make it more adaptive

    virtual void* loadRayList()                                 = 0;
    virtual DynTableProxy loadDynTable(const std::string& name) = 0;
    virtual FixTableProxy loadFixTable(const std::string& name) = 0;

    virtual void loadEntityBVH(BVHType type, const char* prim_type, void** nodes, void** objs) = 0;

    virtual DeviceImageProxy<float> loadImageFromFile(const std::string& filename, int32_t expected_channels)                      = 0;
    virtual DeviceImageProxy<uint8_t> loadPackedImageFromFile(const std::string& filename, int32_t expected_channels, bool linear) = 0;

    virtual DeviceBufferProxy<uint8_t> loadBufferFromFile(const std::string& filename)                     = 0;
    virtual DeviceBufferProxy<uint8_t> loadBufferByName(const std::string& name)                           = 0;
    virtual DeviceBufferProxy<uint8_t> requestBuffer(const std::string& name, int32_t size, int32_t flags) = 0;
    virtual void saveBufferToFile(const std::string& name, const std::string& filename)                    = 0;
    virtual bool copyBufferToHost(const std::string& name, void* dst)                                      = 0;
    virtual bool copyBufferFromHost(const std::string& name, const void* src)                              = 0;

    virtual DeviceImageProxy<float> loadAOVImageForDevice(const std::string& aov_name) = 0;
    virtual DeviceImageProxy<float> loadAOVImageForHost(const std::string& aov_name)   = 0;
    virtual void clearAOV(const std::string& aov_name)                                 = 0;
    virtual void clearAllAOVs()                                                        = 0;

    virtual void runDeviceShader(const TechniqueVariantShaderSet& shaderSet, const Device::RenderSettings& settings) = 0;
    virtual void runTonemapShader(float* in_pixels, uint32_t* device_out_pixels, const TonemapSettings& settings)    = 0;
    virtual ImageInfoOutput runImageInfoShader(float* in_pixels, const ImageInfoSettings& settings)                  = 0;

    virtual void runTraversalShader(TraversalStage stage, int size)                                                            = 0;
    virtual int runRayGenerationShader(int next_id, int size, int xmin, int ymin, int xmax, int ymax)                          = 0;
    virtual void runMaterialShader(int material_id, int first, int last)                                                       = 0;
    virtual void runAdvancedShadowShader(int material_id, int first, int last, bool is_hit)                                    = 0;
    virtual void runCallbackShader(int type)                                                                                   = 0;
    virtual void runBakeShader(const ShaderOutput<void*>& shader, const std::vector<std::string>* resource_map, float* output) = 0;
    virtual void runPassShader(const ShaderOutput<void*>& shader, void* userData)                                              = 0;

    virtual void beginStatsSection(int id)            = 0;
    virtual void endStatsSection(int id)              = 0;
    virtual void addStatsValue(int id, int value)     = 0;
    virtual const Statistics& getAcquiredStatistics() = 0;

    static IDeviceInterface* getCurrentDevice();
    static void setCurrentDevice(IDeviceInterface* interface);
};

} // namespace IG