#include "Device.h"
#include "Image.h"
#include "Logger.h"
#include "RuntimeStructs.h"
#include "Statistics.h"
#include "device/ShaderKey.h"
#include "device/ShallowArray.h"
#include "table/SceneDatabase.h"

#include "generated_interface.h"

#include <anydsl_jit.h>
#include <anydsl_runtime.hpp>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <thread>
#include <type_traits>
#include <variant>

#include <tbb/concurrent_queue.h>

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#ifdef IG_CC_MSC
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif

#define _SECTION(type)                                                  \
    const auto sectionClosure = getThreadData()->stats.section((type)); \
    IG_UNUSED(sectionClosure)

namespace IG {
static const std::string_view DefaultFramebufferName = "Color";

static inline size_t roundUp(size_t num, size_t multiple)
{
    if (multiple == 0)
        return num;

    size_t remainder = num % multiple;
    if (remainder == 0)
        return num;

    return num + multiple - remainder;
}

// TODO: This can be improved in the future with a c++ reflection system
// We assume only pointers will be added to the struct below, else the calculation has to be modified.
constexpr size_t MinPrimaryStreamSize   = (sizeof(PrimaryStream) - sizeof(PrimaryStream::payload)) / sizeof(PrimaryStream::ent_id);
constexpr size_t MinSecondaryStreamSize = (sizeof(SecondaryStream) - sizeof(SecondaryStream::payload)) / sizeof(SecondaryStream::mat_id);

template <typename Node, typename Object>
struct BvhProxy {
    ShallowArray<Node> Nodes;
    ShallowArray<Object> Objs;
};

using Bvh2Ent = BvhProxy<Node2, EntityLeaf1>;
using Bvh4Ent = BvhProxy<Node4, EntityLeaf1>;
using Bvh8Ent = BvhProxy<Node8, EntityLeaf1>;

using BvhVariant = std::variant<Bvh2Ent, Bvh4Ent, Bvh8Ent>;

struct DynTableProxy {
    size_t EntryCount = 0;
    ShallowArray<::LookupEntry> LookupEntries;
    ShallowArray<uint8_t> Data;
};

struct TemporaryStorageHostProxy {
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
    bool Mapped           = false;
    bool HostOnly         = false;
    int IterDiff          = 0; // Addition to the upcoming iteration counter at the end of an iteration
    size_t IterationCount = 0;
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
    size_t BlockSize = 0;
};
using DeviceBuffer = DeviceBufferBase<uint8_t>;
using DeviceStream = DeviceBufferBase<float>;

struct CPUData {
    std::atomic<size_t> ref_count = 0;
    DeviceStream cpu_primary;
    DeviceStream cpu_secondary;
    TemporaryStorageHostProxy temporary_storage_host;
    Statistics stats;
    ParameterSet* current_local_registry = nullptr;
    ShaderKey current_shader_key         = ShaderKey(0, ShaderType::Device, 0);
    std::unordered_map<ShaderKey, ShaderStats, ShaderKeyHash> shader_stats;
};
thread_local CPUData* tlThreadData = nullptr;

#ifdef IG_HAS_DENOISER
void ignis_denoise(Device* device);
#endif

static inline int computeTargetID(const Target& target)
{
    switch (target.gpuArchitecture()) {
    default:
        return 0;
    case GPUArchitecture::Nvidia:
        return ANYDSL_DEVICE(ANYDSL_CUDA, (int)target.device());
    case GPUArchitecture::AMD_HSA:
        return ANYDSL_DEVICE(ANYDSL_HSA, (int)target.device());
    }
}

constexpr size_t GPUStreamBufferCount = 2;
class Interface {
    IG_CLASS_NON_COPYABLE(Interface);
    IG_CLASS_NON_MOVEABLE(Interface);

private:
    class DeviceData {
        IG_CLASS_NON_COPYABLE(DeviceData);

    public:
        DeviceData(DeviceData&& other)            = default;
        DeviceData& operator=(DeviceData&& other) = default;

        std::unordered_map<std::string, BvhVariant> bvh_ents;
        TemporaryStorageHostProxy temporary_storage_host;
        std::array<DeviceStream, GPUStreamBufferCount> primary;
        std::array<DeviceStream, GPUStreamBufferCount> secondary;
        std::unordered_map<std::string, anydsl::Array<float>> aovs;
        anydsl::Array<float> film_pixels;
        anydsl::Array<StreamRay> ray_list;
        std::array<DeviceStream*, GPUStreamBufferCount> current_primary;
        std::array<DeviceStream*, GPUStreamBufferCount> current_secondary;
        std::unordered_map<std::string, DeviceImage> images;
        std::unordered_map<std::string, DevicePackedImage> packed_images;
        std::unordered_map<std::string, DeviceBuffer> buffers;
        std::unordered_map<std::string, DynTableProxy> dyntables;
        std::unordered_map<std::string, DeviceBuffer> fixtables;

        anydsl::Array<uint32_t> tonemap_pixels;

        ParameterSet* current_local_registry = nullptr;
        ShaderKey current_shader_key;

        inline DeviceData()
            : primary()
            , secondary()
            , current_primary()
            , current_secondary()
            , current_shader_key(0, ShaderType::Device, 0)
        {
            setupLinks();
        }

        ~DeviceData() = default;

        inline void setupLinks()
        {
            for (size_t i = 0; i < primary.size(); ++i)
                current_primary[i] = &primary[i];
            for (size_t i = 0; i < secondary.size(); ++i)
                current_secondary[i] = &secondary[i];
        }
    };

    Device const* mIGDevice;
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
    Device::SceneSettings mSceneSettings;
    Device::RenderSettings mCurrentRenderSettings;
    ParameterSet* mCurrentParameters = nullptr;
    TechniqueVariantShaderSet mCurrentShaderSet;
    Settings mCurrentDriverSettings;

    Statistics mMainStats;

    static const Image MissingImage;

    const bool mIsGPU;

public:
    inline explicit Interface(Device* device, const Device::SetupSettings& setup)
        : mIGDevice(device)
        , mDeviceID(computeTargetID(setup.target))
        , mDeviceData()
        , mEntityCount(0)
        , mFramebufferWidth(0)
        , mFramebufferHeight(0)
        , mSetupSettings(setup)
        , mCurrentDriverSettings()
        , mMainStats()
        , mIsGPU(setup.target.isGPU())
    {
        mCurrentDriverSettings.device       = (int)setup.target.device();
        mCurrentDriverSettings.thread_count = (int)setup.target.threadCount();

        updateSettings(Device::RenderSettings{}); // Initialize with default values

        setupThreadData();

        // Special purpose bake shader
        mShaderInfos[ShaderKey(0, ShaderType::Bake, 0)] = {};
    }

    inline ~Interface() = default;

    inline bool isGPU() const { return mIsGPU; }
    inline Target target() const { return mSetupSettings.target; }
    inline int deviceID() const { return mDeviceID; }

    inline size_t framebufferWidth() const { return mFramebufferWidth; }
    inline size_t framebufferHeight() const { return mFramebufferHeight; }
    inline size_t framebufferSize() const { return mFramebufferWidth * mFramebufferHeight; }
    inline bool isInteractive() const { return mSetupSettings.IsInteractive; }
    inline bool hasStatisticAquisition() const { return mSetupSettings.AcquireStats; }

    inline const Device::SceneSettings& sceneSettings() const { return mSceneSettings; }
    inline const Device::RenderSettings& currentRenderSettings() const { return mCurrentRenderSettings; }
    inline const Settings& currentDriverSettings() const { return mCurrentDriverSettings; }

    inline void assignScene(const Device::SceneSettings& settings)
    {
        mSceneSettings = settings;
        mEntityCount   = mSceneSettings.database->FixTables.count("entities") > 0 ? mSceneSettings.database->FixTables.at("entities").entryCount() : 0;
    }

    inline void updateForRender(const TechniqueVariantShaderSet& shaderSet, const Device::RenderSettings& settings, ParameterSet* parameterSet)
    {
        setupShaderSet(shaderSet);
        updateSettings(settings);

        mCurrentRenderSettings = settings;
        mCurrentParameters     = parameterSet;
    }

    inline size_t getPrimaryPayloadBlockSize() const { return mCurrentRenderSettings.info.PrimaryPayloadCount; }
    inline size_t getSecondaryPayloadBlockSize() const { return mCurrentRenderSettings.info.SecondaryPayloadCount; }

    inline const std::string& lookupResource(int32_t id) const
    {
        IG_ASSERT(mSceneSettings.resource_map != nullptr, "Expected resource map to be initialized");
        return mSceneSettings.resource_map->at(id);
    }

    /// @brief Ensure the given buffer is present with the same size as the host. Does not copy from host to device!
    /// @tparam T
    /// @param dev
    /// @param dev_buffer
    /// @param host_buffer
    /// @return
    template <typename T>
    inline anydsl::Array<T>& ensurePresentOnDevice(anydsl::Array<T>& dev_buffer, const anydsl::Array<T>& host_buffer)
    {
        if (dev_buffer.size() != host_buffer.size()) {
            void* ptr = anydsl_alloc(mDeviceID, sizeof(T) * host_buffer.size());
            if (ptr == nullptr) {
                IG_LOG(L_FATAL) << "Out of memory" << std::endl;
                std::abort();
            }
            dev_buffer = std::move(anydsl::Array<T>(mDeviceID, reinterpret_cast<T*>(ptr), host_buffer.size()));
        }

        return dev_buffer;
    }

    template <typename T>
    static inline anydsl::Array<T>& resizeArray(int32_t dev, anydsl::Array<T>& array, size_t size, size_t multiplier)
    {
        const auto capacity = (size & ~((1 << 5) - 1)) + 32; // round to 32
        const size_t n      = capacity * multiplier;
        if (array.size() < (int64_t)n) {
            void* ptr = anydsl_alloc(dev, sizeof(T) * n);
            if (ptr == nullptr) {
                IG_LOG(L_FATAL) << "Out of memory" << std::endl;
                std::abort();
            }
            array = std::move(anydsl::Array<T>(dev, reinterpret_cast<T*>(ptr), n));
        }
        return array;
    }

    inline void ensureFramebuffer()
    {
        const size_t expectedSize = framebufferSize() * 3;

        if (mHostFramebuffer.Data.data() && (size_t)mHostFramebuffer.Data.size() >= expectedSize) {
            resetFramebufferAccess();
            return;
        }

        if (mSceneSettings.aov_map) {
            for (const auto& name : *mSceneSettings.aov_map)
                mAOVs.emplace(name, AOV{});
        }

        mHostFramebuffer.Data = anydsl::Array<float>(expectedSize);
        std::fill(mHostFramebuffer.Data.begin(), mHostFramebuffer.Data.end(), 0.0f);

        for (auto& p : mAOVs) {
            p.second.Data = anydsl::Array<float>(expectedSize);
            std::fill(p.second.Data.begin(), p.second.Data.end(), 0.0f);
        }

        resetFramebufferAccess();
    }

    inline void resizeFramebuffer(size_t width, size_t height)
    {
        IG_ASSERT(width > 0 && height > 0, "Expected given width & height to be greater than 0");

        mFramebufferWidth  = width;
        mFramebufferHeight = height;
        ensureFramebuffer();
    }

    inline void resetFramebufferAccess()
    {
        mHostFramebuffer.Mapped = false;

        for (auto& p : mAOVs)
            p.second.Mapped = false;
    }

    inline void setupThreadData()
    {
        tlThreadData = nullptr;
        mThreadData.clear();

        const size_t req_threads = isGPU() ? 0 : (mSetupSettings.target.threadCount() == 0 ? std::thread::hardware_concurrency() : mSetupSettings.target.threadCount());
        const size_t max_threads = req_threads + 1 /* Host */;

        mAvailableThreadData.clear();
        for (size_t t = 0; t < max_threads; ++t) {
            CPUData* ptr = mThreadData.emplace_back(std::make_unique<CPUData>()).get();
            mAvailableThreadData.push(ptr);
        }
    }

    inline void updateSettings(const Device::RenderSettings& settings)
    {
        // Target specific stuff is NOT updated

        mCurrentDriverSettings.spi    = (int)settings.spi;
        mCurrentDriverSettings.frame  = (int)settings.frame;
        mCurrentDriverSettings.iter   = (int)settings.iteration;
        mCurrentDriverSettings.width  = (int)settings.width;
        mCurrentDriverSettings.height = (int)settings.height;
        mCurrentDriverSettings.seed   = (int)settings.user_seed;

        if (settings.width != mFramebufferWidth || settings.height != mFramebufferHeight)
            resizeFramebuffer(settings.width, settings.height);
    }

    inline void setupShaderSet(const TechniqueVariantShaderSet& shaderSet)
    {
        mCurrentShaderSet = shaderSet;

        // Prepare cache data
        mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::Device, 0));
        if (mCurrentShaderSet.TonemapShader.Exec) {
            mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::Tonemap, 0));
            mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::ImageInfo, 0));
        }

        mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::PrimaryTraversal, 0));
        mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::SecondaryTraversal, 0));
        mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::RayGeneration, 0));
        mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::Miss, 0));
        for (size_t i = 0; i < mCurrentShaderSet.HitShaders.size(); ++i)
            mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::Hit, (uint32)i));
        for (size_t i = 0; i < mCurrentShaderSet.AdvancedShadowHitShaders.size(); ++i)
            mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::AdvancedShadowHit, (uint32)i));
        for (size_t i = 0; i < mCurrentShaderSet.AdvancedShadowMissShaders.size(); ++i)
            mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::AdvancedShadowMiss, (uint32)i));
        for (size_t i = 0; i < mCurrentShaderSet.CallbackShaders.size(); ++i)
            mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::Callback, (uint32)i));
    }

    inline void registerThread()
    {
        if (tlThreadData == nullptr) {
            CPUData* ptr = nullptr;
            while (!mAvailableThreadData.try_pop(ptr))
                std::this_thread::yield();

            if (ptr == nullptr)
                IG_LOG(L_FATAL) << "Registering thread 0x" << std::hex << std::this_thread::get_id() << " failed!" << std::endl;
            else
                tlThreadData = ptr;
        }

        tlThreadData->ref_count++;
    }

    inline void unregisterThread()
    {
        IG_ASSERT(tlThreadData != nullptr, "Expected registerThread together with a unregisterThread");

        if (tlThreadData->ref_count.fetch_sub(1) == 1) {
            mAvailableThreadData.push(tlThreadData);
            tlThreadData = nullptr;
        }
    }

    inline CPUData* getThreadData()
    {
        IG_ASSERT(tlThreadData != nullptr, "Thread not registered");
        return tlThreadData;
    }

    inline void setCurrentShader(int workload, const ShaderKey& key, const ShaderOutput<void*>& shader)
    {
        if (isGPU()) {
            auto data                          = getThreadData();
            mDeviceData.current_local_registry = shader.LocalRegistry.get();
            mDeviceData.current_shader_key     = key;
            data->shader_stats[key].call_count++;
            data->shader_stats[key].workload_count += (size_t)workload;
        } else {
            auto data                    = getThreadData();
            data->current_local_registry = shader.LocalRegistry.get();
            data->current_shader_key     = key;
            data->shader_stats[key].call_count++;
            data->shader_stats[key].workload_count += (size_t)workload;
        }
    }

    inline ParameterSet* getCurrentLocalRegistry()
    {
        if (isGPU())
            return mDeviceData.current_local_registry;
        else
            return getThreadData()->current_local_registry;
    }

    inline ShaderInfo& getCurrentShaderInfo()
    {
        if (isGPU())
            return mShaderInfos.at(mDeviceData.current_shader_key);
        else
            return mShaderInfos.at(getThreadData()->current_shader_key);
    }

    inline DeviceStream& getPrimaryStream(size_t buffer, size_t size)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Get Primary Streams" << std::endl;

        const size_t elements = roundUp(MinPrimaryStreamSize + getPrimaryPayloadBlockSize(), 4);
        auto& stream          = isGPU() ? *mDeviceData.current_primary[buffer] : getThreadData()->cpu_primary;
        resizeArray(mDeviceID, stream.Data, size, elements);
        stream.BlockSize = size;

        return stream;
    }

    inline DeviceStream& getPrimaryStream(size_t buffer)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Get Readonly Primary Streams" << std::endl;

        if (isGPU()) {
            IG_ASSERT(mDeviceData.current_primary[buffer]->Data.size() > 0, "Expected gpu primary stream to be initialized");
            return *mDeviceData.current_primary[buffer];
        } else {
            IG_ASSERT(getThreadData()->cpu_primary.Data.size() > 0, "Expected cpu primary stream to be initialized");
            return getThreadData()->cpu_primary;
        }
    }

    inline DeviceStream& getSecondaryStream(size_t buffer, size_t size)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Get Secondary Streams" << std::endl;

        const size_t elements = roundUp(MinSecondaryStreamSize + getSecondaryPayloadBlockSize(), 4);
        auto& stream          = isGPU() ? *mDeviceData.current_secondary[buffer] : getThreadData()->cpu_secondary;
        resizeArray(mDeviceID, stream.Data, size, elements);
        stream.BlockSize = size;

        return stream;
    }

    inline DeviceStream& getSecondaryStream(size_t buffer)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Get Readonly Secondary Streams" << std::endl;

        if (isGPU()) {
            IG_ASSERT(mDeviceData.current_secondary[buffer]->Data.size() > 0, "Expected gpu secondary stream to be initialized");
            return *mDeviceData.current_secondary[buffer];
        } else {
            IG_ASSERT(getThreadData()->cpu_secondary.Data.size() > 0, "Expected cpu secondary stream to be initialized");
            return getThreadData()->cpu_secondary;
        }
    }

    inline void swapGPUPrimaryStreams()
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Swap GPU Primary Streams" << std::endl;

        std::swap(mDeviceData.current_primary[0], mDeviceData.current_primary[1]);
    }

    inline void swapGPUSecondaryStreams()
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Swap GPU Secondary Streams" << std::endl;

        std::swap(mDeviceData.current_secondary[0], mDeviceData.current_secondary[1]);
    }

    inline size_t getTemporaryBufferSize() const
    {
        // Upper bound extracted from "mapping_*.art"
        return roundUp(std::max<size_t>(32, std::max(mEntityCount + 1, (mSceneSettings.database->MaterialCount + 1) * 2)), 4);
    }

    inline const auto& getTemporaryStorageHost()
    {
        const size_t size = getTemporaryBufferSize();
        if (!isGPU()) {
            auto thread = getThreadData();

            return thread->temporary_storage_host = TemporaryStorageHostProxy{
                std::move(resizeArray(0 /*Host*/, thread->temporary_storage_host.ray_begins, size, 1)),
                std::move(resizeArray(0 /*Host*/, thread->temporary_storage_host.ray_ends, size, 1))
            };
        } else {
            auto& device = mDeviceData;

            return device.temporary_storage_host = TemporaryStorageHostProxy{
                std::move(resizeArray(0 /*Host*/, device.temporary_storage_host.ray_begins, size, 1)),
                std::move(resizeArray(0 /*Host*/, device.temporary_storage_host.ray_ends, size, 1))
            };
        }
    }

    template <typename Bvh, typename Node>
    inline const Bvh& loadEntityBVH(const char* prim_type)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Load Entity BVH" << std::endl;

        std::lock_guard<std::mutex> _guard(mThreadMutex);

        auto& device    = mDeviceData;
        std::string str = prim_type;
        auto it         = device.bvh_ents.find(str);
        if (it == device.bvh_ents.end())
            return std::get<Bvh>(device.bvh_ents.emplace(str, std::move(loadSceneBVH<Node>(prim_type))).first->second);
        else
            return std::get<Bvh>(it->second);
    }

    inline const anydsl::Array<StreamRay>& loadRayList()
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Load Ray List" << std::endl;

        size_t count = mCurrentRenderSettings.width;
        auto& device = mDeviceData;
        if (device.ray_list.size() == (int64_t)count)
            return device.ray_list;

        IG_ASSERT(mCurrentRenderSettings.rays != nullptr, "Expected list of rays to be available");

        std::vector<StreamRay> rays;
        rays.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            const auto dRay = mCurrentRenderSettings.rays[i];

            float norm = dRay.Direction.norm();
            if (norm < std::numeric_limits<float>::epsilon()) {
                IG_LOG(L_ERROR) << "Invalid ray given: Ray has zero direction!" << std::endl;
                norm = 1;
            }

            // FIXME: Bytewise they are essentially the same...
            StreamRay ray;
            ray.org.x = dRay.Origin(0);
            ray.org.y = dRay.Origin(1);
            ray.org.z = dRay.Origin(2);

            ray.dir.x = dRay.Direction(0) / norm;
            ray.dir.y = dRay.Direction(1) / norm;
            ray.dir.z = dRay.Direction(2) / norm;

            ray.tmin = dRay.Range(0);
            ray.tmax = dRay.Range(1);

            rays.push_back(ray);
        }

        return device.ray_list = copyToDevice(rays);
    }

    template <typename T>
    inline anydsl::Array<T> copyToDevice(const T* data, size_t n)
    {
        if (n == 0)
            return anydsl::Array<T>();

        void* ptr = anydsl_alloc(mDeviceID, sizeof(T) * n);
        if (ptr == nullptr) {
            IG_LOG(L_FATAL) << "Out of memory" << std::endl;
            std::abort();
            // return anydsl::Array<T>();
        }

        anydsl::Array<T> array(mDeviceID, reinterpret_cast<T*>(ptr), n);
        anydsl_copy(0, data, 0, mDeviceID, array.data(), 0, sizeof(T) * n);
        return array;
    }

    template <typename T>
    inline anydsl::Array<T> copyToDevice(const std::vector<T>& host)
    {
        return copyToDevice(host.data(), host.size());
    }

    inline DeviceImage copyToDevice(const Image& image)
    {
        IG_ASSERT(image.channels == 1 || image.channels == 4, "Expected image to have one or four channels");
        return DeviceImage{ copyToDevice(image.pixels.get(), image.width * image.height * image.channels), image.width, image.height };
    }

    inline DevicePackedImage copyToDevicePacked(const Image& image)
    {
        std::vector<uint8_t> packed;
        image.copyToPackedFormat(packed);
        return DevicePackedImage{ copyToDevice(packed), image.width, image.height };
    }

    template <typename Node>
    inline BvhProxy<Node, EntityLeaf1> loadSceneBVH(const char* prim_type)
    {
        IG_LOG(L_DEBUG) << "Loading scene bvh " << prim_type << std::endl;
        const auto& bvh         = mSceneSettings.database->SceneBVHs.at(prim_type);
        const size_t node_count = bvh.Nodes.size() / sizeof(Node);
        const size_t leaf_count = bvh.Leaves.size() / sizeof(EntityLeaf1);
        return BvhProxy<Node, EntityLeaf1>{
            std::move(ShallowArray<Node>(mDeviceID, reinterpret_cast<const Node*>(bvh.Nodes.data()), node_count)),
            std::move(ShallowArray<EntityLeaf1>(mDeviceID, reinterpret_cast<const EntityLeaf1*>(bvh.Leaves.data()), leaf_count))
        };
    }

    inline DynTableProxy loadDyntable(const DynTable& tbl)
    {
        static_assert(sizeof(::LookupEntry) == sizeof(LookupEntry), "Expected generated Lookup Entry and internal Lookup Entry to be of same size!");

        DynTableProxy proxy;
        proxy.EntryCount    = tbl.entryCount();
        proxy.LookupEntries = ShallowArray<::LookupEntry>(mDeviceID, (::LookupEntry*)tbl.lookups().data(), tbl.lookups().size());
        proxy.Data          = ShallowArray<uint8_t>(mDeviceID, tbl.data().data(), tbl.data().size());
        return proxy;
    }

    inline const DynTableProxy& loadDyntable(const char* name)
    {
        std::lock_guard<std::mutex> _guard(mThreadMutex);

        auto& tables = mDeviceData.dyntables;
        auto it      = tables.find(name);
        if (it != tables.end())
            return it->second;

        IG_LOG(L_DEBUG) << "Loading dyntable '" << name << "'" << std::endl;

        return tables[name] = loadDyntable(mSceneSettings.database->DynTables.at(name));
    }

    inline DeviceBuffer loadFixtable(const FixTable& tbl)
    {
        return DeviceBuffer{ copyToDevice(tbl.data()), 1 };
    }

    inline const DeviceBuffer& loadFixtable(const char* name)
    {
        std::lock_guard<std::mutex> _guard(mThreadMutex);

        auto& tables = mDeviceData.fixtables;
        auto it      = tables.find(name);
        if (it != tables.end())
            return it->second;

        IG_LOG(L_DEBUG) << "Loading fixtable '" << name << "'" << std::endl;
        IG_ASSERT(mSceneSettings.database->FixTables.count(name) > 0, "Expected given fixtable name to be available");

        return tables[name] = loadFixtable(mSceneSettings.database->FixTables.at(name));
    }

    inline const DeviceImage& loadImage(const std::string& filename, int32_t expected_channels)
    {
        std::lock_guard<std::mutex> _guard(mThreadMutex);

        auto& images = mDeviceData.images;
        auto it      = images.find(filename);
        if (it != images.end())
            return it->second;

        _SECTION(SectionType::ImageLoading);

        IG_LOG(L_DEBUG) << "Loading image '" << filename << "' (C=" << expected_channels << ")" << std::endl;
        try {
            const auto img = Image::load(filename);
            if (expected_channels != (int32_t)img.channels) {
                IG_LOG(L_ERROR) << "Image '" << filename << "' is has unexpected channel count" << std::endl;
                return images[filename] = copyToDevice(Image());
            }

            auto& res = getCurrentShaderInfo().images[filename]; // Get or construct resource info for given resource
            res.counter++;
            res.memory_usage        = img.width * img.height * img.channels * sizeof(float);
            return images[filename] = copyToDevice(img);
        } catch (const ImageLoadException& e) {
            IG_LOG(L_ERROR) << e.what() << std::endl;
            return images[filename] = copyToDevice(MissingImage);
        }
    }

    inline const DevicePackedImage& loadPackedImage(const std::string& filename, int32_t expected_channels, bool linear)
    {
        std::lock_guard<std::mutex> _guard(mThreadMutex);

        auto& images = mDeviceData.packed_images;
        auto it      = images.find(filename);
        if (it != images.end())
            return it->second;

        _SECTION(SectionType::PackedImageLoading);

        IG_LOG(L_DEBUG) << "Loading (packed) image '" << filename << "' (C=" << expected_channels << ")" << std::endl;
        try {
            std::vector<uint8_t> packed;
            size_t width, height, channels;
            Image::loadAsPacked(filename, packed, width, height, channels, linear);

            if (expected_channels != (int32_t)channels) {
                IG_LOG(L_ERROR) << "Packed image '" << filename << "' is has unexpected channel count" << std::endl;
                return images[filename] = copyToDevicePacked(MissingImage);
            }

            auto& res = getCurrentShaderInfo().packed_images[filename]; // Get or construct resource info for given resource
            res.counter++;
            res.memory_usage        = packed.size();
            return images[filename] = DevicePackedImage{ copyToDevice(packed), width, height };
        } catch (const ImageLoadException& e) {
            IG_LOG(L_ERROR) << e.what() << std::endl;
            return images[filename] = copyToDevicePacked(MissingImage);
        }
    }

    std::vector<uint8_t> readBufferFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::binary);
        file.unsetf(std::ios::skipws);

        std::streampos fileSize;
        file.seekg(0, std::ios::end);
        fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> vec;
        vec.reserve(fileSize);

        vec.insert(vec.begin(),
                   std::istream_iterator<uint8_t>(file),
                   std::istream_iterator<uint8_t>());
        return vec;
    }

    inline const DeviceBuffer& loadBuffer(const std::string& filename)
    {
        std::lock_guard<std::mutex> _guard(mThreadMutex);

        auto& buffers = mDeviceData.buffers;
        auto it       = buffers.find(filename);
        if (it != buffers.end())
            return it->second;

        _SECTION(SectionType::BufferLoading);

        IG_LOG(L_DEBUG) << "Loading buffer '" << filename << "'" << std::endl;
        const auto vec = readBufferFile(filename);

        if ((vec.size() % sizeof(int32_t)) != 0)
            IG_LOG(L_WARNING) << "Buffer '" << filename << "' is not properly sized!" << std::endl;

        return buffers[filename] = DeviceBuffer{ copyToDevice(vec), 1 };
    }

    inline DeviceBuffer& requestBuffer(const std::string& name, int32_t size, int32_t flags)
    {
        IG_UNUSED(flags); // We do not make use of it yet

        std::lock_guard<std::mutex> _guard(mThreadMutex);

        IG_ASSERT(size > 0, "Expected buffer size to be larger then zero");

        // Make sure the buffer is properly sized
        size = (int32_t)roundUp(size, 32);

        auto& buffers = mDeviceData.buffers;
        if (auto it = buffers.find(name); it != buffers.end() && it->second.Data.size() >= (int64_t)size)
            return it->second;

        _SECTION(SectionType::BufferRequests);

        IG_LOG(L_DEBUG) << "Requested buffer '" << name << "' with " << FormatMemory(size) << std::endl;

        void* ptr = anydsl_alloc(mDeviceID, size);
        if (ptr == nullptr) {
            IG_LOG(L_FATAL) << "Out of memory" << std::endl;
            std::abort();
        }

        return buffers[name] = DeviceBuffer{ anydsl::Array<uint8_t>(mDeviceID, reinterpret_cast<uint8_t*>(ptr), size), 1 };
    }

    inline void releaseBuffer(const std::string& name)
    {
        std::lock_guard<std::mutex> _guard(mThreadMutex);

        auto& buffers = mDeviceData.buffers;

        if (auto it = buffers.find(name); it != buffers.end()) {
            _SECTION(SectionType::BufferReleases);
            buffers.erase(it);
        }
    }

    inline size_t getBufferSize(const std::string& buffer_name)
    {
        return getBufferForDevice(buffer_name).SizeInBytes;
    }

    inline bool copyBufferToHost(const std::string& buffer_name, void* dst, size_t maxSizeByte)
    {
        const auto acc = getBufferForDevice(buffer_name);
        if (!acc.Data)
            return false;

        size_t size = std::min(acc.SizeInBytes, maxSizeByte);
        if (size == 0)
            return false;

        anydsl_copy(mDeviceID, acc.Data, 0, 0 /* Host */, dst, 0, size);

        return true;
    }

    inline Device::BufferAccessor getBufferForDevice(const std::string& buffer_name)
    {
        auto& buffers = mDeviceData.buffers;
        if (auto it = buffers.find(buffer_name); it != buffers.end()) {
            const size_t size = (size_t)it->second.Data.size();

            return Device::BufferAccessor{ .Data = it->second.Data.data(), .SizeInBytes = size };
        }
        return Device::BufferAccessor{ .Data = nullptr, .SizeInBytes = 0 };
    }

    inline void dumpBuffer(const std::string& name, const std::string& filename)
    {
        std::lock_guard<std::mutex> _guard(mThreadMutex);

        auto& buffers = mDeviceData.buffers;
        if (auto it = buffers.find(name); it != buffers.end()) {
            const size_t size = (size_t)it->second.Data.size();

            IG_LOG(L_DEBUG) << "Dumping buffer '" << name << "' to '" << filename << "' with " << FormatMemory(size) << std::endl;

            // Copy data to host
            std::vector<uint8_t> host_data(size);
            anydsl_copy(mDeviceID, it->second.Data.data(), 0, 0 /* Host */, host_data.data(), 0, host_data.size());

            // Dump data as binary glob
            std::ofstream out(filename);
            out.write(reinterpret_cast<const char*>(host_data.data()), host_data.size());
            out.close();
        } else {
            IG_LOG(L_WARNING) << "Buffer '" << name << "' can not be dumped as it does not exists" << std::endl;
        }
    }

    inline void dumpDebugBuffer(DeviceBuffer& buffer)
    {
        const auto& handleDebug = [](int32_t* ptr, int32_t occup) {
            for (int32_t k = 0; k < occup; ++k) {
                int32_t op = ptr[k + 1];

                if (op == 1) { // Print string
                    ++k;
                    const char* sptr = reinterpret_cast<const char*>(ptr);
                    bool atEnd       = false;
                    while (true) {
                        for (int i = 0; i < 4; ++i) {
                            const char c = sptr[4 * (k + 1) + i];
                            if (c == 0) {
                                atEnd = true;
                                break;
                            } else {
                                std::cout << c;
                            }
                        }
                        if (atEnd)
                            break;
                        ++k;
                    }
                } else if (op == 2) { // Print i32
                    ++k;
                    std::cout << ptr[k + 1];
                } else if (op == 3) { // Print f32
                    ++k;
                    std::cout << reinterpret_cast<const float*>(ptr)[k + 1];
                } else {
                    break;
                }
            }

            std::cout << std::flush;

            // Reset data
            ptr[0] = 0;
        };

        if (isGPU()) {
            // Copy data to host
            std::vector<uint8_t> host_data((size_t)buffer.Data.size());
            anydsl_copy(mDeviceID, buffer.Data.data(), 0, 0 /* Host */, host_data.data(), 0, host_data.size());

            // Parse data
            int32_t* ptr  = reinterpret_cast<int32_t*>(host_data.data());
            int32_t occup = std::min(ptr[0], static_cast<int32_t>(host_data.size() / sizeof(int32_t)));

            if (occup <= 0)
                return;

            handleDebug(ptr, occup);

            // Copy back to device
            anydsl_copy(0 /* Host */, host_data.data(), 0, mDeviceID, buffer.Data.data(), 0, sizeof(int32_t));
        } else {
            // Already on the host
            int32_t* ptr  = reinterpret_cast<int32_t*>(buffer.Data.data());
            int32_t occup = std::min(ptr[0], static_cast<int32_t>(buffer.Data.size() / sizeof(int32_t)));

            if (occup <= 0)
                return;

            handleDebug(ptr, occup);
        }
    }

    inline void checkDebugOutput()
    {
        if (const auto it = mDeviceData.buffers.find("__dbg_output"); it != mDeviceData.buffers.end())
            dumpDebugBuffer(it->second);
    }

    /// @brief Will release almost all device specific data
    inline void releaseAll()
    {
        std::lock_guard<std::mutex> _guard(mThreadMutex);
        DeviceData data;
        std::swap(data, mDeviceData);

        mDeviceData.setupLinks(); // Reconnect
    }

    // -------------------------------------------------------- Shader
    inline void runDeviceShader()
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Device Shader" << std::endl;

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::Device, 1, {});

        using Callback = decltype(ig_callback_shader);
        auto callback  = reinterpret_cast<Callback*>(mCurrentShaderSet.DeviceShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected device shader to be valid");
        setCurrentShader(1, ShaderKey(mCurrentShaderSet.ID, ShaderType::Device, 0), mCurrentShaderSet.DeviceShader);
        callback(&mCurrentDriverSettings);

        checkDebugOutput();

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.endShaderLaunch(ShaderType::Device, {});
    }

    inline void runTonemapShader(float* in_pixels, uint32_t* device_out_pixels, ::TonemapSettings& settings)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Tonemap Shader" << std::endl;

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::Tonemap, 1, {});

        using Callback = decltype(ig_tonemap_shader);
        auto callback  = reinterpret_cast<Callback*>(mCurrentShaderSet.TonemapShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected tonemap shader to be valid");
        setCurrentShader(1, ShaderKey(mCurrentShaderSet.ID, ShaderType::Tonemap, 0), mCurrentShaderSet.TonemapShader);
        callback(&mCurrentDriverSettings, in_pixels, device_out_pixels, (int)mFramebufferWidth, (int)mFramebufferHeight, &settings);

        checkDebugOutput();

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.endShaderLaunch(ShaderType::Tonemap, {});
    }
    
    inline ::ImageInfoOutput runImageinfoShader(float* in_pixels, ::ImageInfoSettings& settings)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Imageinfo Shader" << std::endl;

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::ImageInfo, 1, {});

        using Callback = decltype(ig_imageinfo_shader);
        auto callback  = reinterpret_cast<Callback*>(mCurrentShaderSet.ImageinfoShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected imageinfo shader to be valid");
        setCurrentShader(1, ShaderKey(mCurrentShaderSet.ID, ShaderType::ImageInfo, 0), mCurrentShaderSet.ImageinfoShader);

        ::ImageInfoOutput output;
        callback(&mCurrentDriverSettings, in_pixels, (int)mFramebufferWidth, (int)mFramebufferHeight, &settings, &output);

        checkDebugOutput();

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.endShaderLaunch(ShaderType::ImageInfo, {});

        return output;
    }

    inline void runPrimaryTraversalShader(int size)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Primary Traversal Shader [S=" << size << "]" << std::endl;

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::PrimaryTraversal, size, {});

        using Callback = decltype(ig_traversal_shader);
        auto callback  = reinterpret_cast<Callback*>(mCurrentShaderSet.PrimaryTraversalShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected primary traversal shader to be valid");
        setCurrentShader(size, ShaderKey(mCurrentShaderSet.ID, ShaderType::PrimaryTraversal, 0), mCurrentShaderSet.PrimaryTraversalShader);
        callback(&mCurrentDriverSettings, size);

        checkDebugOutput();

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.endShaderLaunch(ShaderType::PrimaryTraversal, {});
    }

    inline void runSecondaryTraversalShader(int size)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Secondary Traversal Shader [S=" << size << "]" << std::endl;

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::SecondaryTraversal, size, {});

        using Callback = decltype(ig_traversal_shader);
        auto callback  = reinterpret_cast<Callback*>(mCurrentShaderSet.SecondaryTraversalShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected secondary traversal shader to be valid");
        setCurrentShader(size, ShaderKey(mCurrentShaderSet.ID, ShaderType::SecondaryTraversal, 0), mCurrentShaderSet.SecondaryTraversalShader);
        callback(&mCurrentDriverSettings, size);

        checkDebugOutput();

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.endShaderLaunch(ShaderType::SecondaryTraversal, {});
    }

    inline int runRayGenerationShader(int next_id, int size, int xmin, int ymin, int xmax, int ymax)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Ray Generation Shader [S=" << size << ", I=" << next_id << "]" << std::endl;

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::RayGeneration, (xmax - xmin) * (ymax - ymin), {});

        using Callback = decltype(ig_ray_generation_shader);
        auto callback  = reinterpret_cast<Callback*>(mCurrentShaderSet.RayGenerationShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected ray generation shader to be valid");
        setCurrentShader((xmax - xmin) * (ymax - ymin), ShaderKey(mCurrentShaderSet.ID, ShaderType::RayGeneration, 0), mCurrentShaderSet.RayGenerationShader);
        const int ret = callback(&mCurrentDriverSettings, next_id, size, xmin, ymin, xmax, ymax);

        checkDebugOutput();

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.endShaderLaunch(ShaderType::RayGeneration, {});
        return ret;
    }

    inline void runMissShader(int first, int last)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Miss Shader [S=" << first << ", E=" << last << "]" << std::endl;

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::Miss, last - first, {});

        using Callback = decltype(ig_miss_shader);
        auto callback  = reinterpret_cast<Callback*>(mCurrentShaderSet.MissShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected miss shader to be valid");
        setCurrentShader(last - first, ShaderKey(mCurrentShaderSet.ID, ShaderType::Miss, 0), mCurrentShaderSet.MissShader);
        callback(&mCurrentDriverSettings, first, last);

        checkDebugOutput();

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.endShaderLaunch(ShaderType::Miss, {});
    }

    inline void runHitShader(int material_id, int first, int last)
    {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Hit Shader [M=" << material_id << ", S=" << first << ", E=" << last << "]" << std::endl;

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::Hit, last - first, material_id);

        using Callback = decltype(ig_hit_shader);
        IG_ASSERT(material_id >= 0 && material_id < (int)mCurrentShaderSet.HitShaders.size(), "Expected material id for hit shaders to be valid");
        const auto& output = mCurrentShaderSet.HitShaders.at(material_id);
        auto callback      = reinterpret_cast<Callback*>(output.Exec);
        IG_ASSERT(callback != nullptr, "Expected hit shader to be valid");
        setCurrentShader(last - first, ShaderKey(mCurrentShaderSet.ID, ShaderType::Hit, (uint32)material_id), output);
        callback(&mCurrentDriverSettings, material_id, first, last);

        checkDebugOutput();

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.endShaderLaunch(ShaderType::Hit, material_id);
    }

    inline bool useAdvancedShadowHandling()
    {
        return !mCurrentShaderSet.AdvancedShadowHitShaders.empty() && !mCurrentShaderSet.AdvancedShadowMissShaders.empty();
    }

    inline void runAdvancedShadowShader(int material_id, int first, int last, bool is_hit)
    {
        IG_ASSERT(useAdvancedShadowHandling(), "Expected advanced shadow shader only be called if it is enabled!");

        if (is_hit) {
            if (mSetupSettings.DebugTrace)
                IG_LOG(L_DEBUG) << "TRACE> Advanced Hit Shader [I=" << material_id << ", S=" << first << ", E=" << last << "]" << std::endl;

            if (mSetupSettings.AcquireStats)
                getThreadData()->stats.beginShaderLaunch(ShaderType::AdvancedShadowHit, last - first, material_id);

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(material_id >= 0 && material_id < (int)mCurrentShaderSet.AdvancedShadowHitShaders.size(), "Expected material id for advanced shadow hit shaders to be valid");
            const auto& output = mCurrentShaderSet.AdvancedShadowHitShaders.at(material_id);
            auto callback      = reinterpret_cast<Callback*>(output.Exec);
            IG_ASSERT(callback != nullptr, "Expected advanced shadow hit shader to be valid");
            setCurrentShader(last - first, ShaderKey(mCurrentShaderSet.ID, ShaderType::AdvancedShadowHit, (uint32)material_id), output);
            callback(&mCurrentDriverSettings, material_id, first, last);

            checkDebugOutput();

            if (mSetupSettings.AcquireStats)
                getThreadData()->stats.endShaderLaunch(ShaderType::AdvancedShadowHit, material_id);
        } else {
            if (mSetupSettings.DebugTrace)
                IG_LOG(L_DEBUG) << "TRACE> Advanced Miss Shader [I=" << material_id << ", S=" << first << ", E=" << last << "]" << std::endl;

            if (mSetupSettings.AcquireStats)
                getThreadData()->stats.beginShaderLaunch(ShaderType::AdvancedShadowMiss, last - first, material_id);

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(material_id >= 0 && material_id < (int)mCurrentShaderSet.AdvancedShadowMissShaders.size(), "Expected material id for advanced shadow miss shaders to be valid");
            const auto& output = mCurrentShaderSet.AdvancedShadowMissShaders.at(material_id);
            auto callback      = reinterpret_cast<Callback*>(output.Exec);
            IG_ASSERT(callback != nullptr, "Expected advanced shadow miss shader to be valid");
            setCurrentShader(last - first, ShaderKey(mCurrentShaderSet.ID, ShaderType::Miss, (uint32)material_id), output);
            callback(&mCurrentDriverSettings, material_id, first, last);

            checkDebugOutput();

            if (mSetupSettings.AcquireStats)
                getThreadData()->stats.endShaderLaunch(ShaderType::AdvancedShadowMiss, material_id);
        }
    }

    inline void runCallbackShader(int type)
    {
        IG_ASSERT(type >= 0 && type < (int)CallbackType::_COUNT, "Expected callback shader type to be well formed!");

        const auto& output = mCurrentShaderSet.CallbackShaders[type];
        using Callback     = decltype(ig_callback_shader);
        auto callback      = reinterpret_cast<Callback*>(output.Exec);

        if (callback != nullptr) {
            if (mSetupSettings.DebugTrace)
                IG_LOG(L_DEBUG) << "TRACE> Callback Shader [T=" << type << "]" << std::endl;

            if (mSetupSettings.AcquireStats)
                getThreadData()->stats.beginShaderLaunch(ShaderType::Callback, 1, type);

            setCurrentShader(1, ShaderKey(mCurrentShaderSet.ID, ShaderType::Callback, (uint32)type), output);
            callback(&mCurrentDriverSettings);

            checkDebugOutput();

            if (mSetupSettings.AcquireStats)
                getThreadData()->stats.endShaderLaunch(ShaderType::Callback, type);
        }
    }

    inline void runBakeShader(const ShaderOutput<void*>& shader, const std::vector<std::string>* resource_map, float* output)
    {
        IG_ASSERT(shader.Exec != nullptr, "Expected bake shader to be valid");

        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Bake Shader" << std::endl;

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::Bake, 1, {});

        const auto copy             = mSceneSettings.resource_map;
        mSceneSettings.resource_map = resource_map;

        using Callback = decltype(ig_bake_shader);
        auto callback  = reinterpret_cast<Callback*>(shader.Exec);

        setCurrentShader(1, ShaderKey(0, ShaderType::Bake, 0), shader);
        callback(&mCurrentDriverSettings, output);

        checkDebugOutput();

        mSceneSettings.resource_map = copy;

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.endShaderLaunch(ShaderType::Bake, {});
    }

    inline void runPassShader(const ShaderOutput<void*>& shader)
    {
        IG_ASSERT(shader.Exec != nullptr, "Expected pass shader to be valid");

        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Pass Shader" << std::endl;

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::Pass, 1, {});

        using Callback = decltype(ig_pass_main);
        auto callback  = reinterpret_cast<Callback*>(shader.Exec);

        setCurrentShader(1, ShaderKey(0, ShaderType::Pass, 0), shader);
        callback(&mCurrentDriverSettings);

        checkDebugOutput();

        if (mSetupSettings.AcquireStats)
            getThreadData()->stats.endShaderLaunch(ShaderType::Pass, {});
    }

    // ---------------------------------------------------- Framebuffer/AOV stuff

    inline anydsl::Array<float> createFramebuffer(int dev) const
    {
        auto film_size = framebufferSize() * 3;
        void* ptr      = anydsl_alloc(dev, sizeof(float) * film_size);
        if (ptr == nullptr) {
            IG_LOG(L_FATAL) << "Out of memory" << std::endl;
            std::abort();
            // return anydsl::Array<float>();
        }

        auto film_data = reinterpret_cast<float*>(ptr);
        return anydsl::Array<float>(dev, film_data, film_size);
    }

    inline float* getMainFramebuffer()
    {
        IG_ASSERT(mHostFramebuffer.Data.data() != nullptr, "Expected host framebuffer to be already initialized");

        if (isGPU()) {
            auto& device = mDeviceData;
            if (device.film_pixels.size() != mHostFramebuffer.Data.size()) {
                _SECTION(SectionType::FramebufferUpdate);
                device.film_pixels = createFramebuffer(mDeviceID);
                anydsl::copy(mHostFramebuffer.Data, device.film_pixels);
            }
            return device.film_pixels.data();
        } else {
            return mHostFramebuffer.Data.data();
        }
    }

    inline void mapAOVToDevice(const std::string& name, const AOV& host, bool onlyAtCreation = true)
    {
        if (!isGPU()) // Device is host
            return;

        bool created = false;
        auto& device = mDeviceData;
        if (device.aovs[name].size() != host.Data.size()) {
            _SECTION(SectionType::AOVUpdate);
            device.aovs[name] = createFramebuffer(mDeviceID);
            created           = true;
        }

        if (!onlyAtCreation || created)
            anydsl::copy(host.Data, device.aovs[name]);
    }

    inline Device::AOVAccessor getAOVImageOnlyCPU(const std::string& aov_name)
    {
        IG_ASSERT(!isGPU(), "Should only be called if not GPU");

        if (aov_name.empty() || aov_name == DefaultFramebufferName)
            return Device::AOVAccessor{ getMainFramebuffer(), mHostFramebuffer.IterationCount };

        const auto it = mAOVs.find(aov_name);
        if (it == mAOVs.end()) {
            IG_LOG(L_ERROR) << "Unknown aov '" << aov_name << "' access" << std::endl;
            return Device::AOVAccessor{ nullptr, 0 };
        }

        return Device::AOVAccessor{ it->second.Data.data(), it->second.IterationCount };
    }

    inline uint32_t* getTonemapImageGPU()
    {
        const size_t film_size = framebufferSize();
        auto& device           = mDeviceData;
        if ((size_t)device.tonemap_pixels.size() != film_size) {
            _SECTION(SectionType::TonemapUpdate);

            void* ptr = anydsl_alloc(mDeviceID, sizeof(uint32_t) * film_size);
            if (ptr == nullptr) {
                IG_LOG(L_FATAL) << "Out of memory" << std::endl;
                std::abort();
                return nullptr;
            }
            auto film_data        = reinterpret_cast<uint32_t*>(ptr);
            device.tonemap_pixels = anydsl::Array<uint32_t>(mDeviceID, film_data, film_size);
        }
        return device.tonemap_pixels.data();
    }

    inline Device::AOVAccessor getAOVImageForHost(const std::string& aov_name)
    {
        if (!mHostFramebuffer.Data.data()) {
            IG_LOG(L_ERROR) << "Framebuffer not yet initialized. Run a single iteration first" << std::endl;
            return Device::AOVAccessor{ nullptr, 0 };
        }

        if (isGPU()) {
            if (aov_name.empty() || aov_name == DefaultFramebufferName) {
                if (!mHostFramebuffer.Mapped && mDeviceData.film_pixels.data() != nullptr) {
                    _SECTION(SectionType::FramebufferHostUpdate);
                    anydsl::copy(mDeviceData.film_pixels, mHostFramebuffer.Data);
                    mHostFramebuffer.Mapped = true;
                }
                return Device::AOVAccessor{ mHostFramebuffer.Data.data(), mHostFramebuffer.IterationCount };
            } else {
                const auto it = mAOVs.find(aov_name);
                if (it == mAOVs.end()) {
                    IG_LOG(L_ERROR) << "Unknown aov '" << aov_name << "' access for host" << std::endl;
                    return Device::AOVAccessor{ nullptr, 0 };
                }

                if (!it->second.HostOnly && !it->second.Mapped && mDeviceData.aovs[aov_name].data() != nullptr) {
                    _SECTION(SectionType::AOVHostUpdate);
                    anydsl::copy(mDeviceData.aovs[aov_name], it->second.Data);
                    it->second.Mapped = true;
                }
                return Device::AOVAccessor{ it->second.Data.data(), it->second.IterationCount };
            }
        } else {
            return getAOVImageOnlyCPU(aov_name);
        }
    }

    inline Device::AOVAccessor getAOVImageForDevice(const std::string& aov_name)
    {
        if (!mHostFramebuffer.Data.data()) {
            IG_LOG(L_ERROR) << "Framebuffer not yet initialized. Run a single iteration first" << std::endl;
            return Device::AOVAccessor{ nullptr, 0 };
        }

        if (isGPU()) {
            if (aov_name.empty() || aov_name == DefaultFramebufferName) {
                return Device::AOVAccessor{ mDeviceData.film_pixels.data(), mHostFramebuffer.IterationCount };
            } else {
                const auto it = mAOVs.find(aov_name);
                if (it == mAOVs.end()) {
                    IG_LOG(L_ERROR) << "Unknown aov '" << aov_name << "' access" << std::endl;
                    return Device::AOVAccessor{ nullptr, 0 };
                }

                return Device::AOVAccessor{ ensurePresentOnDevice(mDeviceData.aovs[aov_name], mAOVs[aov_name].Data).data(), it->second.IterationCount };
            }
        } else {
            return getAOVImageOnlyCPU(aov_name);
        }
    }

    inline void markAOVAsUsed(const char* name, int iter)
    {
        const std::lock_guard<std::mutex> guard(mThreadMutex);

        const std::string aov_name = name ? std::string(name) : std::string{};
        if (aov_name.empty() || aov_name == DefaultFramebufferName) {
            mHostFramebuffer.IterDiff = iter;
        } else {
            const auto it = mAOVs.find(aov_name);
            if (it == mAOVs.end())
                IG_LOG(L_ERROR) << "Unknown aov '" << aov_name << "' access" << std::endl;
            else
                it->second.IterDiff = iter;
        }
    }

    /// Clear all aovs and the framebuffer
    inline void clearAllAOVs()
    {
        clearAOV({});
        for (const auto& p : mAOVs)
            clearAOV(p.first.c_str());
    }

    /// Clear specific aov
    inline void clearAOV(const std::string& aov_name)
    {
        if (!mHostFramebuffer.Data.data())
            return;

        if (aov_name.empty() || aov_name == DefaultFramebufferName) {
            mHostFramebuffer.IterationCount = 0;
            mHostFramebuffer.IterDiff       = 0;
            std::memset(mHostFramebuffer.Data.data(), 0, sizeof(float) * mHostFramebuffer.Data.size());
            if (mDeviceData.film_pixels.size() == mHostFramebuffer.Data.size())
                anydsl::copy(mHostFramebuffer.Data, mDeviceData.film_pixels);
        } else {
            auto& aov          = mAOVs.at(aov_name);
            aov.IterationCount = 0;
            aov.IterDiff       = 0;
            auto& buffer       = aov.Data;
            std::memset(buffer.data(), 0, sizeof(float) * buffer.size());
            if (const auto it = mDeviceData.aovs.find(aov_name); it != mDeviceData.aovs.end()) {
                if (it->second.size() == buffer.size())
                    anydsl::copy(buffer, it->second);
            }
        }
    }
#ifdef IG_HAS_DENOISER
    inline void denoise()
    {
        if (aovs.count("Denoised") == 0 && mHostFramebuffer.IterationCount > 0)
            return;

        ignis_denoise(mIGDevice);

        // Make sure the iteration count resembles the input
        auto& outputAOV          = aovs["Denoised"];
        outputAOV.IterationCount = mHostFramebuffer.IterationCount;
        outputAOV.IterDiff       = 0;
    }
#endif

    inline void present()
    {
        // Single thread access
        if (!mCurrentRenderSettings.info.LockFramebuffer) {
            mHostFramebuffer.IterationCount = (size_t)std::max<int64_t>(0, (int64_t)mHostFramebuffer.IterationCount + 1);
            mHostFramebuffer.IterDiff       = 0;
        }

        for (auto& p : mAOVs) {
            p.second.IterationCount = (size_t)std::max<int64_t>(0, (int64_t)p.second.IterationCount + p.second.IterDiff);
            p.second.IterDiff       = 0;
        }
    }

    inline Statistics* getFullStats()
    {
        mMainStats.reset();
        for (const auto& data : mThreadData)
            mMainStats.add(data->stats);

        return &mMainStats;
    }

    // Access parameters
    int getParameterInt(const char* name, int def, bool global)
    {
        const ParameterSet* registry = global ? mCurrentParameters : getCurrentLocalRegistry();
        // IG_ASSERT(registry != nullptr, "No parameters available!"); // Might be unavailable in baking process
        if (registry && registry->IntParameters.count(name) > 0)
            return registry->IntParameters.at(name);
        else
            return def;
    }

    float getParameterFloat(const char* name, float def, bool global)
    {
        const ParameterSet* registry = global ? mCurrentParameters : getCurrentLocalRegistry();
        // IG_ASSERT(registry != nullptr, "No parameters available!");
        if (registry && registry->FloatParameters.count(name) > 0)
            return registry->FloatParameters.at(name);
        else
            return def;
    }

    void getParameterVector(const char* name, float defX, float defY, float defZ, float& outX, float& outY, float& outZ, bool global)
    {
        const ParameterSet* registry = global ? mCurrentParameters : getCurrentLocalRegistry();
        // IG_ASSERT(registry != nullptr, "No parameters available!");
        if (registry && registry->VectorParameters.count(name) > 0) {
            Vector3f param = registry->VectorParameters.at(name);
            outX           = param.x();
            outY           = param.y();
            outZ           = param.z();
        } else {
            outX = defX;
            outY = defY;
            outZ = defZ;
        }
    }

    void getParameterColor(const char* name, float defR, float defG, float defB, float defA, float& outR, float& outG, float& outB, float& outA, bool global)
    {
        const ParameterSet* registry = global ? mCurrentParameters : getCurrentLocalRegistry();
        // IG_ASSERT(registry != nullptr, "No parameters available!");
        if (registry && registry->ColorParameters.count(name) > 0) {
            Vector4f param = registry->ColorParameters.at(name);
            outR           = param.x();
            outG           = param.y();
            outB           = param.z();
            outA           = param.w();
        } else {
            outR = defR;
            outG = defG;
            outB = defB;
            outA = defA;
        }
    }

    void setParameterInt(const char* name, int value, bool global)
    {
        ParameterSet* registry = global ? mCurrentParameters : getCurrentLocalRegistry();
        if (registry)
            registry->IntParameters[name] = value;
    }

    void setParameterFloat(const char* name, float value, bool global)
    {
        ParameterSet* registry = global ? mCurrentParameters : getCurrentLocalRegistry();
        if (registry)
            registry->FloatParameters[name] = value;
    }

    void setParameterVector(const char* name, float valueX, float valueY, float valueZ, bool global)
    {
        ParameterSet* registry = global ? mCurrentParameters : getCurrentLocalRegistry();
        if (registry)
            registry->VectorParameters[name] = Vector3f(valueX, valueY, valueZ);
    }

    void setParameterColor(const char* name, float valueR, float valueG, float valueB, float valueA, bool global)
    {
        ParameterSet* registry = global ? mCurrentParameters : getCurrentLocalRegistry();
        if (registry)
            registry->ColorParameters[name] = Vector4f(valueR, valueG, valueB, valueA);
    }
};

const Image Interface::MissingImage = Image::createSolidImage(Vector4f(1, 0, 1, 1));
static std::unique_ptr<Interface> sInterface;

// --------------------- Math stuff
[[maybe_unused]] thread_local unsigned int stPrevMathMode = 0;
static inline void enableMathMode()
{
    // Force flush to zero mode for denormals
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
    stPrevMathMode = _mm_getcsr();
    _mm_setcsr(stPrevMathMode | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
#endif
}

static inline void disableMathMode()
{
    // Reset mode
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
    _mm_setcsr(stPrevMathMode);
#endif
}

// --------------------- Device
Device::Device(const Device::SetupSettings& settings)
{
    IG_ASSERT(sInterface == nullptr, "Only a single instance allowed!");
    sInterface = std::make_unique<Interface>(this, settings);

    IG_LOG(L_INFO) << "Using device " << anydsl_device_name(sInterface->deviceID()) << std::endl;

    if (settings.target.isCPU() && settings.target.vectorWidth() > 1)
        IG_LOG(L_WARNING) << "CPU device with vector width > 1 is experimental and might crash!" << std::endl;
}

Device::~Device()
{
    sInterface.reset();
}

Target Device::target() const { return sInterface->target(); }

size_t Device::framebufferWidth() const { return sInterface->framebufferWidth(); }

size_t Device::framebufferHeight() const { return sInterface->framebufferHeight(); }

bool Device::isInteractive() const { return sInterface->isInteractive(); }

void Device::assignScene(const SceneSettings& settings)
{
    sInterface->assignScene(settings);
}

static inline void enterDevice()
{
    enableMathMode();
    sInterface->registerThread();
    sInterface->ensureFramebuffer();
}

static inline void leaveDevice()
{
    sInterface->unregisterThread();
    disableMathMode();
}

void Device::render(const TechniqueVariantShaderSet& shaderSet, const Device::RenderSettings& settings, ParameterSet* parameterSet)
{
    enterDevice();

    sInterface->updateForRender(shaderSet, settings, parameterSet);

    sInterface->ensureFramebuffer();
    sInterface->runDeviceShader();
    sInterface->present();

#ifdef IG_HAS_DENOISER
    if (settings.denoise)
        sInterface->denoise();
#endif

    leaveDevice();
}

void Device::resize(size_t width, size_t height)
{
    sInterface->resizeFramebuffer(width, height);
}

void Device::releaseAll()
{
    sInterface->releaseAll();
}

Device::AOVAccessor Device::getFramebufferForHost(const std::string& name)
{
    sInterface->registerThread();
    const auto acc = sInterface->getAOVImageForHost(name);
    sInterface->unregisterThread();
    return acc;
}

Device::AOVAccessor Device::getFramebufferForDevice(const std::string& name)
{
    sInterface->registerThread();
    const auto acc = sInterface->getAOVImageForDevice(name);
    sInterface->unregisterThread();
    return acc;
}

void Device::clearAllFramebuffer()
{
    sInterface->clearAllAOVs();
}

void Device::clearFramebuffer(const std::string& name)
{
    sInterface->clearAOV(name);
}

size_t Device::getBufferSizeInBytes(const std::string& name)
{
    sInterface->registerThread();
    const size_t size = sInterface->getBufferSize(name);
    sInterface->unregisterThread();
    return size;
}

bool Device::copyBufferToHost(const std::string& name, void* dst, size_t maxSizeByte)
{
    sInterface->registerThread();
    const auto successful = sInterface->copyBufferToHost(name, dst, maxSizeByte);
    sInterface->unregisterThread();
    return successful;
}

Device::BufferAccessor Device::getBufferForDevice(const std::string& name)
{
    sInterface->registerThread();
    const auto acc = sInterface->getBufferForDevice(name);
    sInterface->unregisterThread();
    return acc;
}

const Statistics* Device::getStatistics()
{
    return sInterface->getFullStats();
}

void Device::tonemap(uint32_t* out_pixels, const TonemapSettings& driver_settings)
{
    enterDevice();

    const auto acc       = sInterface->getAOVImageForDevice(driver_settings.AOV);
    float* in_pixels     = acc.Data;
    const float inv_iter = acc.IterationCount > 0 ? 1.0f / acc.IterationCount : 0.0f;

    uint32_t* device_out_pixels = sInterface->isGPU() ? sInterface->getTonemapImageGPU() : out_pixels;

    ::TonemapSettings settings;
    settings.method          = (int)driver_settings.Method;
    settings.use_gamma       = driver_settings.UseGamma;
    settings.scale           = driver_settings.Scale * inv_iter;
    settings.exposure_factor = driver_settings.ExposureFactor;
    settings.exposure_offset = driver_settings.ExposureOffset;

    sInterface->runTonemapShader(in_pixels, device_out_pixels, settings);

    if (sInterface->isGPU()) {
        size_t size = sInterface->framebufferSize();
        anydsl_copy(sInterface->deviceID(), device_out_pixels, 0, 0 /* Host */, out_pixels, 0, sizeof(uint32_t) * size);
    }

    leaveDevice();
}

ImageInfoOutput Device::imageinfo(const ImageInfoSettings& driver_settings)
{
    enterDevice();

    const auto acc       = sInterface->getAOVImageForDevice(driver_settings.AOV);
    float* in_pixels     = acc.Data;
    const float inv_iter = acc.IterationCount > 0 ? 1.0f / acc.IterationCount : 0.0f;

    ::ImageInfoSettings settings;
    settings.scale               = driver_settings.Scale * inv_iter;
    settings.bins                = (int)driver_settings.Bins;
    settings.histogram_r         = driver_settings.HistogramR;
    settings.histogram_g         = driver_settings.HistogramG;
    settings.histogram_b         = driver_settings.HistogramB;
    settings.histogram_l         = driver_settings.HistogramL;
    settings.acquire_error_stats = driver_settings.AcquireErrorStats;
    settings.acquire_histogram   = driver_settings.AcquireHistogram;

    ::ImageInfoOutput output = sInterface->runImageinfoShader(in_pixels, settings);

    ImageInfoOutput driver_output;
    driver_output.Min      = output.min;
    driver_output.Max      = output.max;
    driver_output.Average  = output.avg;
    driver_output.SoftMin  = output.soft_min;
    driver_output.SoftMax  = output.soft_max;
    driver_output.Median   = output.median;
    driver_output.InfCount = output.inf_counter;
    driver_output.NaNCount = output.nan_counter;
    driver_output.NegCount = output.neg_counter;

    leaveDevice();

    return driver_output;
}

void Device::bake(const ShaderOutput<void*>& shader, const std::vector<std::string>* resource_map, float* output)
{
    enterDevice();

    sInterface->runBakeShader(shader, resource_map, output);

    leaveDevice();
}

void Device::runPass(const ShaderOutput<void*>& shader)
{
    enterDevice();

    sInterface->runPassShader(shader);

    leaveDevice();
}

template <typename T>
inline void get_stream(T* dev_stream, DeviceStream& stream, size_t min_components)
{
    static_assert(std::is_standard_layout<T>::value, "Expected stream to be plain old data");
    static_assert((sizeof(T) % sizeof(float*)) == 0, "Expected stream size to be multiple of pointer size");

    float* ptr      = stream.Data.data();
    size_t capacity = stream.BlockSize;

    auto r_ptr = reinterpret_cast<float**>(dev_stream);
    for (size_t i = 0; i <= min_components; ++i) // The last part of the stream is used by the payload
        r_ptr[i] = ptr + i * capacity;
}

} // namespace IG

using IG::sInterface;
extern "C" {
IG_EXPORT void ignis_get_film_data(float** pixels, int* width, int* height)
{
    *pixels = sInterface->getMainFramebuffer();
    *width  = (int)sInterface->framebufferWidth();
    *height = (int)sInterface->framebufferHeight();
}

IG_EXPORT void ignis_get_aov_image(const char* name, float** aov_pixels)
{
    *aov_pixels = sInterface->getAOVImageForDevice(name).Data;
}

IG_EXPORT void ignis_mark_aov_as_used(const char* name, int iter)
{
    sInterface->markAOVAsUsed(name, iter);
}

IG_EXPORT void ignis_get_work_info(WorkInfo* info)
{
    if (sInterface->currentDriverSettings().width > 0 && sInterface->currentDriverSettings().height > 0) {
        info->width  = (int)sInterface->currentDriverSettings().width;
        info->height = (int)sInterface->currentDriverSettings().height;
    } else {
        info->width  = (int)sInterface->framebufferWidth();
        info->height = (int)sInterface->framebufferHeight();
    }

    info->advanced_shadows                = sInterface->useAdvancedShadowHandling() && sInterface->currentRenderSettings().info.ShadowHandlingMode == IG::ShadowHandlingMode::Advanced;
    info->advanced_shadows_with_materials = sInterface->useAdvancedShadowHandling() && sInterface->currentRenderSettings().info.ShadowHandlingMode == IG::ShadowHandlingMode::AdvancedWithMaterials;
    info->framebuffer_locked              = sInterface->currentRenderSettings().info.LockFramebuffer;
}

IG_EXPORT void ignis_load_bvh2_ent(const char* prim_type, Node2** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<IG::Bvh2Ent, Node2>(prim_type);
    *nodes    = const_cast<Node2*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

IG_EXPORT void ignis_load_bvh4_ent(const char* prim_type, Node4** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<IG::Bvh4Ent, Node4>(prim_type);
    *nodes    = const_cast<Node4*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

IG_EXPORT void ignis_load_bvh8_ent(const char* prim_type, Node8** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<IG::Bvh8Ent, Node8>(prim_type);
    *nodes    = const_cast<Node8*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

static inline DynTableData assignDynTable(const IG::DynTableProxy& tbl)
{
    DynTableData devtbl;
    devtbl.count  = tbl.EntryCount;
    devtbl.header = const_cast<LookupEntry*>(tbl.LookupEntries.ptr());
    uint64_t size = tbl.Data.size();
    devtbl.size   = size;
    devtbl.start  = const_cast<uint8_t*>(tbl.Data.ptr());
    return devtbl;
}

IG_EXPORT void ignis_load_dyntable(const char* name, DynTableData* dtb)
{
    auto& proxy = sInterface->loadDyntable(name);
    *dtb        = assignDynTable(proxy);
}

IG_EXPORT void ignis_load_fixtable(const char* name, uint8_t** data, int32_t* size)
{
    auto& buf = sInterface->loadFixtable(name);
    *data     = const_cast<uint8_t*>(buf.Data.data());
    *size     = (int32_t)buf.Data.size();
}

IG_EXPORT void ignis_load_rays(StreamRay** list)
{
    *list = const_cast<StreamRay*>(sInterface->loadRayList().data());
}

IG_EXPORT void ignis_load_image(const char* file, float** pixels, int32_t* width, int32_t* height, int32_t expected_channels)
{
    auto& img = sInterface->loadImage(file, expected_channels);
    *pixels   = const_cast<float*>(img.Data.data());
    *width    = (int32_t)img.Width;
    *height   = (int32_t)img.Height;
}

IG_EXPORT void ignis_load_image_by_id(int32_t id, float** pixels, int32_t* width, int32_t* height, int32_t expected_channels)
{
    return ignis_load_image(sInterface->lookupResource(id).c_str(), pixels, width, height, expected_channels);
}

IG_EXPORT void ignis_load_packed_image(const char* file, uint8_t** pixels, int32_t* width, int32_t* height, int32_t expected_channels, bool linear)
{
    auto& img = sInterface->loadPackedImage(file, expected_channels, linear);
    *pixels   = const_cast<uint8_t*>(img.Data.data());
    *width    = (int32_t)img.Width;
    *height   = (int32_t)img.Height;
}

IG_EXPORT void ignis_load_packed_image_by_id(int32_t id, uint8_t** pixels, int32_t* width, int32_t* height, int32_t expected_channels, bool linear)
{
    return ignis_load_packed_image(sInterface->lookupResource(id).c_str(), pixels, width, height, expected_channels, linear);
}

IG_EXPORT void ignis_load_buffer(const char* file, uint8_t** data, int32_t* size)
{
    auto& img = sInterface->loadBuffer(file);
    *data     = const_cast<uint8_t*>(img.Data.data());
    *size     = (int32_t)img.Data.size();
}

IG_EXPORT void ignis_load_buffer_by_id(int32_t id, uint8_t** data, int32_t* size)
{
    return ignis_load_buffer(sInterface->lookupResource(id).c_str(), data, size);
}

IG_EXPORT void ignis_request_buffer(const char* name, uint8_t** data, int size, int flags)
{
    auto& buffer = sInterface->requestBuffer(name, size, flags);
    *data        = const_cast<uint8_t*>(buffer.Data.data());
}

IG_EXPORT void ignis_dbg_dump_buffer(const char* name, const char* filename)
{
    sInterface->dumpBuffer(name, filename);
}

IG_EXPORT void ignis_get_temporary_storage_host(TemporaryStorageHost* temp)
{
    const auto& data          = sInterface->getTemporaryStorageHost();
    temp->ray_begins          = const_cast<int32_t*>(data.ray_begins.data());
    temp->ray_ends            = const_cast<int32_t*>(data.ray_ends.data());
    temp->entity_per_material = const_cast<int32_t*>(sInterface->sceneSettings().entity_per_material->data());
}

IG_EXPORT void ignis_get_primary_stream(int id, PrimaryStream* primary, int size)
{
    auto& stream = sInterface->getPrimaryStream(id, size);
    IG::get_stream(primary, stream, IG::MinPrimaryStreamSize);
}

IG_EXPORT void ignis_get_primary_stream_const(int id, PrimaryStream* primary)
{
    auto& stream = sInterface->getPrimaryStream(id);
    IG::get_stream(primary, stream, IG::MinPrimaryStreamSize);
}

IG_EXPORT void ignis_get_secondary_stream(int id, SecondaryStream* secondary, int size)
{
    auto& stream = sInterface->getSecondaryStream(id, size);
    IG::get_stream(secondary, stream, IG::MinSecondaryStreamSize);
}

IG_EXPORT void ignis_get_secondary_stream_const(int id, SecondaryStream* secondary)
{
    auto& stream = sInterface->getSecondaryStream(id);
    IG::get_stream(secondary, stream, IG::MinSecondaryStreamSize);
}

IG_EXPORT void ignis_gpu_swap_primary_streams()
{
    sInterface->swapGPUPrimaryStreams();
}

IG_EXPORT void ignis_gpu_swap_secondary_streams()
{
    sInterface->swapGPUSecondaryStreams();
}

IG_EXPORT void ignis_register_thread()
{
    IG::enableMathMode();
    sInterface->registerThread();
}

IG_EXPORT void ignis_unregister_thread()
{
    sInterface->unregisterThread();
    IG::disableMathMode();
}

IG_EXPORT void ignis_handle_traverse_primary(int size)
{
    sInterface->runPrimaryTraversalShader(size);
}

IG_EXPORT void ignis_handle_traverse_secondary(int size)
{
    sInterface->runSecondaryTraversalShader(size);
}

IG_EXPORT int ignis_handle_ray_generation(int next_id, int size, int xmin, int ymin, int xmax, int ymax)
{
    return sInterface->runRayGenerationShader(next_id, size, xmin, ymin, xmax, ymax);
}

IG_EXPORT void ignis_handle_miss_shader(int first, int last)
{
    sInterface->runMissShader(first, last);
}

IG_EXPORT void ignis_handle_hit_shader(int entity_id, int first, int last)
{
    sInterface->runHitShader(entity_id, first, last);
}

IG_EXPORT void ignis_handle_advanced_shadow_shader(int material_id, int first, int last, bool is_hit)
{
    if (sInterface->currentRenderSettings().info.ShadowHandlingMode == IG::ShadowHandlingMode::Advanced)
        sInterface->runAdvancedShadowShader(0 /* Fix to 0 */, first, last, is_hit);
    else
        sInterface->runAdvancedShadowShader(material_id, first, last, is_hit);
}

IG_EXPORT void ignis_handle_callback_shader(int type)
{
    sInterface->runCallbackShader(type);
}

// Registry stuff
IG_EXPORT int ignis_get_parameter_i32(const char* name, int def, bool global)
{
    return sInterface->getParameterInt(name, def, global);
}

IG_EXPORT float ignis_get_parameter_f32(const char* name, float def, bool global)
{
    return sInterface->getParameterFloat(name, def, global);
}

IG_EXPORT void ignis_get_parameter_vector(const char* name, float defX, float defY, float defZ, float* x, float* y, float* z, bool global)
{
    sInterface->getParameterVector(name, defX, defY, defZ, *x, *y, *z, global);
}

IG_EXPORT void ignis_get_parameter_color(const char* name, float defR, float defG, float defB, float defA, float* r, float* g, float* b, float* a, bool global)
{
    sInterface->getParameterColor(name, defR, defG, defB, defA, *r, *g, *b, *a, global);
}

IG_EXPORT void ignis_set_parameter_i32(const char* name, int value, bool global)
{
    sInterface->setParameterInt(name, value, global);
}

IG_EXPORT void ignis_set_parameter_f32(const char* name, float value, bool global)
{
    sInterface->setParameterFloat(name, value, global);
}

IG_EXPORT void ignis_set_parameter_vector(const char* name, float valueX, float valueY, float valueZ, bool global)
{
    sInterface->setParameterVector(name, valueX, valueY, valueZ, global);
}

IG_EXPORT void ignis_set_parameter_color(const char* name, float valueR, float valueG, float valueB, float valueA, bool global)
{
    sInterface->setParameterColor(name, valueR, valueG, valueB, valueA, global);
}

// Stats
IG_EXPORT void ignis_stats_begin_section(int id)
{
    if (!sInterface->hasStatisticAquisition())
        return;

    sInterface->getThreadData()->stats.beginSection((IG::SectionType)id);
}

IG_EXPORT void ignis_stats_end_section(int id)
{
    if (!sInterface->hasStatisticAquisition())
        return;

    sInterface->getThreadData()->stats.endSection((IG::SectionType)id);
}

IG_EXPORT void ignis_stats_add(int id, int value)
{
    if (!sInterface->hasStatisticAquisition())
        return;

    sInterface->getThreadData()->stats.increase((IG::Quantity)id, static_cast<uint64_t>(value));
}
}
