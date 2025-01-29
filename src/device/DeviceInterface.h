#pragma once

#include "Statistics.h"
#include "device/IDeviceInterface.h"
#include "device/ShaderKey.h"
#include "device/ShallowArray.h"

#include "generated_interface.h"

#include <anydsl_runtime.hpp>

#include <tbb/concurrent_queue.h>

namespace IG {

template <typename Node, typename Object>
struct BvhProxy {
    ShallowArray<Node> Nodes;
    ShallowArray<Object> Objs;
};

using Bvh2Ent = BvhProxy<Node2, EntityLeaf1>;
using Bvh4Ent = BvhProxy<Node4, EntityLeaf1>;
using Bvh8Ent = BvhProxy<Node8, EntityLeaf1>;

using BvhVariant = std::variant<Bvh2Ent, Bvh4Ent, Bvh8Ent>;

struct DeviceDynTable {
    size_t EntryCount = 0;
    ShallowArray<::LookupEntry> LookupEntries;
    ShallowArray<uint8_t> Data;
};

struct TemporaryStorageHost {
    anydsl::Array<int32_t> ray_begins;
    anydsl::Array<int32_t> ray_ends;
};

struct Resource {
    size_t counter; // Number of uses
    size_t memory_usage;
};

struct ShaderInfo {
    std::unordered_map<std::string, Resource> images;
    std::unordered_map<std::string, Resource> packed_images;
};

struct ShaderStats {
    size_t call_count     = 0;
    size_t workload_count = 0;
};

struct AOV {
    anydsl::Array<float> Data;
    /// If true, host & device are out of sync
    bool Dirty = false;
};

template <typename T>
struct DeviceImageBase {
    anydsl::Array<T> Data;
    size_t Width  = 0;
    size_t Height = 0;
};
using DeviceImage       = DeviceImageBase<float>;
using DevicePackedImage = DeviceImageBase<uint8_t>; // Packed RGBA

template <typename T>
struct DeviceBufferBase {
    anydsl::Array<T> Data;
};
using DeviceBuffer = DeviceBufferBase<uint8_t>;

template <typename T>
struct DeviceStreamBase {
    anydsl::Array<T> Data;
    size_t BlockSize = 0;
};
using DeviceStream = DeviceStreamBase<float>;

struct CPUData {
    std::atomic<size_t> ref_count = 0;
    std::array<DeviceStream, (size_t)IDeviceInterface::StreamType::Count> streams;
    TemporaryStorageHost temporary_storage_host;
    Statistics stats;
    ParameterSet* current_local_registry = nullptr;
    ShaderKey current_shader_key         = ShaderKey(0, ShaderType::Device, 0);
    std::unordered_map<ShaderKey, ShaderStats, ShaderKeyHash> shader_stats;
};

constexpr size_t GPUStreamBufferCount = 2;

class DeviceInterface : public IDeviceInterface {
public:
    class DeviceData {
        IG_CLASS_NON_COPYABLE(DeviceData);

    public:
        DeviceData(DeviceData&& other)            = default;
        DeviceData& operator=(DeviceData&& other) = default;

        std::unordered_map<std::string, BvhVariant> bvh_ents;
        TemporaryStorageHost temporary_storage_host;
        std::array<DeviceStream, GPUStreamBufferCount*(size_t)StreamType::Count> streams;
        std::unordered_map<std::string, anydsl::Array<float>> aovs;
        anydsl::Array<float> film_pixels;
        anydsl::Array<StreamRay> ray_list;
        std::array<DeviceStream*, GPUStreamBufferCount*(size_t)StreamType::Count> current_streams;
        std::unordered_map<std::string, DeviceImage> images;
        std::unordered_map<std::string, DevicePackedImage> packed_images;
        std::unordered_map<std::string, DeviceBuffer> buffers;
        std::unordered_map<std::string, DeviceDynTable> dyntables;
        std::unordered_map<std::string, DeviceBuffer> fixtables;

        ParameterSet* current_local_registry = nullptr;
        ShaderKey current_shader_key;

        inline DeviceData()
            : streams()
            , current_streams()
            , current_shader_key(0, ShaderType::Device, 0)
        {
            setupLinks();
        }

        ~DeviceData() = default;

        inline void setupLinks()
        {
            for (size_t i = 0; i < streams.size(); ++i)
                current_streams[i] = &streams[i];
        }
    };

    explicit DeviceInterface(const Device::SetupSettings& settings);
    virtual ~DeviceInterface();

    bool isGPU() const override;
    Target target() const override;
    int deviceID() const override;

    void releaseAllMemory() override;

    void connectGlobalRegistry(ParameterSet* parameter_set) override;
    void setCurrentSceneSettings(const Device::SceneSettings&) override;
    const Device::SceneSettings& currentSceneSettings() const override;
    const Device::RenderSettings& currentRenderSettings() const override;

    std::pair<size_t, size_t> framebufferSize() const override;
    std::pair<size_t, size_t> workSize() const override;

    DeviceImageProxy<float> getFramebuffer() override;
    void resizeFramebuffer(size_t width, size_t height) override;

    std::string lookupResource(int32_t id) const override;

    void registerThread() override;
    void unregisterThread() override;

    ParameterSet* getCurrentGlobalRegistry() override;
    ParameterSet* getCurrentLocalRegistry() override;

    DeviceStreamProxy<float> getStream(StreamType type, size_t buffer_id, size_t size, size_t minComponents) override;
    DeviceStreamProxy<float> getStream(StreamType type, size_t buffer_id) override;
    void swapGPUStreams(StreamType type) override;

    TemporaryStorageHostProxy getTemporaryStorageHost() override; // TODO: Would be cool to get rid of this and make it more adaptive

    void* loadRayList() override;
    DynTableProxy loadDynTable(const std::string& name) override;
    FixTableProxy loadFixTable(const std::string& name) override;

    void loadEntityBVH(BVHType type, const char* prim_type, void** nodes, void** objs) override;

    DeviceImageProxy<float> loadImageFromFile(const std::string& filename, int32_t expected_channels) override;
    DeviceImageProxy<uint8_t> loadPackedImageFromFile(const std::string& filename, int32_t expected_channels, bool linear) override;

    DeviceBufferProxy<uint8_t> loadBufferFromFile(const std::string& filename) override;
    DeviceBufferProxy<uint8_t> loadBufferByName(const std::string& name) override;
    DeviceBufferProxy<uint8_t> requestBuffer(const std::string& name, int32_t size, int32_t flags) override;
    void saveBuffer(const std::string& name, const std::string& filename) override;
    bool copyBufferToHost(const std::string& name, void* dst, size_t maxSizeByte) override;
    bool copyBufferFromHost(const std::string& name, const void* src, size_t maxSizeByte) override;

    DeviceImageProxy<float> loadAOVImageForDevice(const std::string& aov_name) override;
    DeviceImageProxy<float> loadAOVImageForHost(const std::string& aov_name) override;
    void clearAOV(const std::string& aov_name) override;
    void clearAllAOVs() override;
    void mapAOVBackToDevice(const std::string& aov_name) override;
    void mapAllAOVsBackToDevice() override;

    void runDeviceShader(const TechniqueVariantShaderSet& shaderSet, const Device::RenderSettings& settings) override;
    void runTonemapShader(float* in_pixels, uint32_t* device_out_pixels, const TonemapSettings& settings) override;
    ImageInfoOutput runImageInfoShader(float* in_pixels, const ImageInfoSettings& settings) override;

    void runTraversalShader(TraversalStage stage, int size) override;
    int runRayGenerationShader(int next_id, int size, int xmin, int ymin, int xmax, int ymax) override;
    void runMaterialShader(int material_id, int first, int last) override;
    void runAdvancedShadowShader(int material_id, int first, int last, bool is_hit) override;
    void runCallbackShader(int type) override;
    void runBakeShader(const ShaderOutput<void*>& shader, const std::vector<std::string>* resource_map, float* output) override;
    void runPassShader(const ShaderOutput<void*>& shader, void* userData) override;

    void beginStatsSection(int id) override;
    void endStatsSection(int id) override;
    void addStatsValue(int id, int value) override;
    const Statistics& getAcquiredStatistics() override;

    void enterDevice();
    void leaveDevice();

private:
    void ensureFramebuffer();
    void resetFramebufferAccess();
    anydsl::Array<float> createFramebuffer(int dev) const;

    void updateSettings(const Device::RenderSettings& settings);
    void updateShaderSet(const TechniqueVariantShaderSet& shaderSet);

    void setCurrentShader(int workload, const ShaderKey& key, const ShaderOutput<void*>& shader);
    ShaderInfo& getCurrentShader();

    void setupThreadData();
    CPUData* getCurrentThreadData() const;

    void handleDebugOutput();

    DeviceImageProxy<float> loadAOVImageForCPU(const std::string& aov_name);

    void updateStatistics();

    const int mDeviceID;
    DeviceData mDeviceData;

    std::mutex mThreadMutex;
    std::vector<std::unique_ptr<CPUData>> mThreadData;

    tbb::concurrent_queue<CPUData*> mAvailableThreadData;
    std::unordered_map<ShaderKey, ShaderInfo, ShaderKeyHash> mShaderInfos;

    std::unordered_map<std::string, AOV> mAOVs;
    AOV mHostFramebuffer;

    size_t mEntityCount;
    size_t mFramebufferWidth;
    size_t mFramebufferHeight;

    const Device::SetupSettings mSetupSettings;
    Device::SceneSettings mCurrentSceneSettings;
    Device::RenderSettings mCurrentRenderSettings;
    ParameterSet* mCurrentParameters = nullptr;
    TechniqueVariantShaderSet mCurrentShaderSet;
    ::Settings mCurrentDriverSettings;

    Statistics mAcquiredStats;

    const bool mIsGPU;
};
} // namespace IG