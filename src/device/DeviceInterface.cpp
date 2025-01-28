#include "DeviceInterface.h"
#include "Image.h"
#include "Logger.h"
#include "RuntimeStructs.h"
#include "device/DeviceUtils.h"
#include "table/SceneDatabase.h"

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

#define _SECTION(type)                                                         \
    const auto sectionClosure = getCurrentThreadData()->stats.section((type)); \
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

/// @brief Ensure the given buffer is present with the same size as the host. Does not copy from host to device!
/// @tparam T
/// @param dev
/// @param dev_buffer
/// @param host_buffer
/// @return
template <typename T>
inline anydsl::Array<T>& ensurePresentOnDevice(int32_t dev, anydsl::Array<T>& dev_buffer, const anydsl::Array<T>& host_buffer)
{
    if (dev_buffer.size() != host_buffer.size()) {
        void* ptr = anydsl_alloc(dev, sizeof(T) * host_buffer.size());
        if (ptr == nullptr) {
            IG_LOG(L_FATAL) << "Out of memory" << std::endl;
            std::abort();
        }
        dev_buffer = std::move(anydsl::Array<T>(dev, reinterpret_cast<T*>(ptr), host_buffer.size()));
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

template <typename T>
inline anydsl::Array<T> copyToDevice(int32_t dev, const T* data, size_t n)
{
    if (n == 0)
        return anydsl::Array<T>();

    void* ptr = anydsl_alloc(dev, sizeof(T) * n);
    if (ptr == nullptr) {
        IG_LOG(L_FATAL) << "Out of memory" << std::endl;
        std::abort();
        // return anydsl::Array<T>();
    }

    anydsl::Array<T> array(dev, reinterpret_cast<T*>(ptr), n);
    anydsl_copy(0, data, 0, dev, array.data(), 0, sizeof(T) * n);
    return array;
}

template <typename T>
inline anydsl::Array<T> copyToDevice(int32_t dev, const std::vector<T>& host)
{
    return copyToDevice(dev, host.data(), host.size());
}

inline DeviceImage copyToDevice(int32_t dev, const Image& image)
{
    IG_ASSERT(image.channels == 1 || image.channels == 4, "Expected image to have one or four channels");
    return DeviceImage{ copyToDevice(dev, image.pixels.get(), image.width * image.height * image.channels), image.width, image.height };
}

inline DevicePackedImage copyToDevicePacked(int32_t dev, const Image& image)
{
    std::vector<uint8_t> packed;
    image.copyToPackedFormat(packed);
    return DevicePackedImage{ copyToDevice(dev, packed), image.width, image.height };
}

template <typename T>
inline IDeviceInterface::DeviceBufferProxy<T> mapToProxy(const DeviceBufferBase<T>& buffer)
{
    return IDeviceInterface::DeviceBufferProxy<T>{
        .DataPtr   = const_cast<T*>(buffer.Data.data()),
        .DataSize  = (size_t)buffer.Data.size(),
        .BlockSize = buffer.BlockSize
    };
}

template <typename T>
inline IDeviceInterface::DeviceImageProxy<T> mapToProxy(const DeviceImageBase<T>& buffer)
{
    return IDeviceInterface::DeviceImageProxy<T>{
        .DataPtr = const_cast<T*>(buffer.Data.data()),
        .Width   = buffer.Width,
        .Height  = buffer.Height
    };
}

static const Image MissingImage = Image::createSolidImage(Vector4f(1, 0, 1, 1));

/// @brief Guard to ensure `getCurrentThreadData()` and fast math.
/// Note, the call to _SECTION makes use of `getCurrentThreadData()`!
class DeviceGuard {
    DeviceInterface* const mInterface;

public:
    inline explicit DeviceGuard(DeviceInterface* interface)
        : mInterface(interface)
    {
        mInterface->enterDevice();
    }
    inline ~DeviceGuard()
    {
        mInterface->leaveDevice();
    }
};

DeviceInterface::DeviceInterface(const Device::SetupSettings& setup)
    : mDeviceID(computeTargetID(setup.Target))
    , mDeviceData()
    , mEntityCount(0)
    , mFramebufferWidth(0)
    , mFramebufferHeight(0)
    , mSetupSettings(setup)
    , mCurrentDriverSettings()
    , mAcquiredStats()
    , mIsGPU(setup.Target.isGPU())
{

    IG_LOG(L_INFO) << "Using device " << anydsl_device_name(mDeviceID) << std::endl;

    mCurrentDriverSettings.device       = (int)setup.Target.device();
    mCurrentDriverSettings.thread_count = (int)setup.Target.threadCount();

    updateSettings(Device::RenderSettings{}); // Initialize with default values

    setupThreadData();

    // Special purpose bake shader
    mShaderInfos[ShaderKey(0, ShaderType::Bake, 0)] = {};
}

DeviceInterface::~DeviceInterface()
{
}

bool DeviceInterface::isGPU() const { return mIsGPU; }
Target DeviceInterface::target() const { return mSetupSettings.Target; }
int DeviceInterface::deviceID() const { return mDeviceID; }

void DeviceInterface::releaseAllMemory()
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);
    DeviceData data;
    std::swap(data, mDeviceData);

    mDeviceData.setupLinks(); // Reconnect
}

std::pair<size_t, size_t> DeviceInterface::framebufferSize() const { return { mFramebufferWidth, mFramebufferHeight }; }
std::pair<size_t, size_t> DeviceInterface::workSize() const
{
    if (mCurrentRenderSettings.width > 0 && mCurrentRenderSettings.height > 0)
        return { mCurrentRenderSettings.width, mCurrentRenderSettings.height };
    else
        return framebufferSize();
}

const Device::SceneSettings& DeviceInterface::currentSceneSettings() const { return mCurrentSceneSettings; }
const Device::RenderSettings& DeviceInterface::currentRenderSettings() const { return mCurrentRenderSettings; }

void DeviceInterface::setCurrentSceneSettings(const Device::SceneSettings& settings)
{
    mCurrentSceneSettings = settings;
    mEntityCount          = mCurrentSceneSettings.database->FixTables.count("entities") > 0 ? mCurrentSceneSettings.database->FixTables.at("entities").entryCount() : 0;
}

void DeviceInterface::updateContext(const TechniqueVariantShaderSet& shaderSet, const Device::RenderSettings& settings, ParameterSet* parameterSet)
{
    updateShaderSet(shaderSet);
    updateSettings(settings);

    mCurrentRenderSettings = settings;
    mCurrentParameters     = parameterSet;

    resetFramebufferAccess();
}

IDeviceInterface::DeviceImageProxy<float> DeviceInterface::getFramebuffer()
{
    DeviceGuard _guard(this);
    ensureFramebuffer();

    IG_ASSERT(mHostFramebuffer.Data.data() != nullptr, "Expected host framebuffer to be already initialized");

    if (isGPU()) {
        auto& device = mDeviceData;
        if (device.film_pixels.size() != mHostFramebuffer.Data.size()) {
            _SECTION(SectionType::FramebufferUpdate);
            device.film_pixels = createFramebuffer(mDeviceID);
            anydsl::copy(mHostFramebuffer.Data, device.film_pixels);
        }

        return {
            .DataPtr = device.film_pixels.data(),
            .Width   = mFramebufferWidth,
            .Height  = mFramebufferHeight
        };
    } else {
        return {
            .DataPtr = mHostFramebuffer.Data.data(),
            .Width   = mFramebufferWidth,
            .Height  = mFramebufferHeight
        };
    }
}

void DeviceInterface::resizeFramebuffer(size_t width, size_t height)
{
    IG_ASSERT(width > 0 && height > 0, "Expected given width & height to be greater than 0");

    mFramebufferWidth  = width;
    mFramebufferHeight = height;
    ensureFramebuffer();
}

void DeviceInterface::ensureFramebuffer()
{
    const size_t expectedSize = framebufferArea() * 3;

    IG_ASSERT(expectedSize > 0, "Expected host framebuffer to have a valid size");

    if (mHostFramebuffer.Data.data() && (size_t)mHostFramebuffer.Data.size() >= expectedSize)
        return;

    if (mCurrentSceneSettings.aov_map) {
        for (const auto& name : *mCurrentSceneSettings.aov_map)
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

void DeviceInterface::resetFramebufferAccess()
{
    mHostFramebuffer.Dirty = true;

    for (auto& p : mAOVs)
        p.second.Dirty = true;
}

anydsl::Array<float> DeviceInterface::createFramebuffer(int dev) const
{
    auto film_size = framebufferArea() * 3;
    void* ptr      = anydsl_alloc(dev, sizeof(float) * film_size);
    if (ptr == nullptr) {
        IG_LOG(L_FATAL) << "Out of memory" << std::endl;
        std::abort();
        // return anydsl::Array<float>();
    }

    auto film_data = reinterpret_cast<float*>(ptr);
    return anydsl::Array<float>(dev, film_data, film_size);
}

std::string DeviceInterface::lookupResource(int32_t id) const
{
    IG_ASSERT(mCurrentSceneSettings.resource_map != nullptr, "Expected resource map to be initialized");
    if (id < 0 || (size_t)id >= mCurrentSceneSettings.resource_map->size()) {
        IG_LOG(L_ERROR) << "Resource ID '" << id << "' is not known!" << std::endl;
        return {};
    }
    return mCurrentSceneSettings.resource_map->at(id);
}

thread_local CPUData* tlThreadData = nullptr;
void DeviceInterface::setupThreadData()
{
    tlThreadData = nullptr;
    mThreadData.clear();

    const size_t req_threads = isGPU() ? 0 : (mSetupSettings.Target.threadCount() == 0 ? std::thread::hardware_concurrency() : mSetupSettings.Target.threadCount());
    const size_t max_threads = req_threads + 1 /* Host */;

    mAvailableThreadData.clear();
    for (size_t t = 0; t < max_threads; ++t) {
        CPUData* ptr = mThreadData.emplace_back(std::make_unique<CPUData>()).get();
        mAvailableThreadData.push(ptr);
    }
}

CPUData* DeviceInterface::getCurrentThreadData() const
{
    IG_ASSERT(tlThreadData != nullptr, "Thread not registered");
    return tlThreadData;
}

void DeviceInterface::registerThread()
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

void DeviceInterface::unregisterThread()
{
    IG_ASSERT(tlThreadData != nullptr, "Expected registerThread together with a unregisterThread");

    if (tlThreadData->ref_count.fetch_sub(1) == 1) {
        mAvailableThreadData.push(tlThreadData);
        tlThreadData = nullptr;
    }
}

ParameterSet* DeviceInterface::getCurrentGlobalRegistry() { return mCurrentParameters; }

ParameterSet* DeviceInterface::getCurrentLocalRegistry()
{
    DeviceGuard _guard(this);
    if (isGPU())
        return mDeviceData.current_local_registry;
    else
        return getCurrentThreadData()->current_local_registry;
}

void DeviceInterface::updateSettings(const Device::RenderSettings& settings)
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

void DeviceInterface::updateShaderSet(const TechniqueVariantShaderSet& shaderSet)
{
    mCurrentShaderSet = shaderSet;

    // Prepare cache data
    mShaderInfos.clear();

    mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::Device, 0));
    if (mCurrentShaderSet.TonemapShader.Exec) {
        mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::Tonemap, 0));
        mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::ImageInfo, 0));
    }

    mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::PrimaryTraversal, 0));
    mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::SecondaryTraversal, 0));
    mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::RayGeneration, 0));
    mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::Miss, -1));
    for (size_t i = 0; i < mCurrentShaderSet.HitShaders.size(); ++i)
        mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::Hit, (uint32)i));
    for (size_t i = 0; i < mCurrentShaderSet.AdvancedShadowHitShaders.size(); ++i)
        mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::AdvancedShadowHit, (uint32)i));
    for (size_t i = 0; i < mCurrentShaderSet.AdvancedShadowMissShaders.size(); ++i)
        mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::AdvancedShadowMiss, (uint32)i));
    for (size_t i = 0; i < mCurrentShaderSet.CallbackShaders.size(); ++i)
        mShaderInfos.try_emplace(ShaderKey(mCurrentShaderSet.ID, ShaderType::Callback, (uint32)i));
}

void DeviceInterface::setCurrentShader(int workload, const ShaderKey& key, const ShaderOutput<void*>& shader)
{
    if (isGPU()) {
        auto data                          = getCurrentThreadData();
        mDeviceData.current_local_registry = shader.LocalRegistry.get();
        mDeviceData.current_shader_key     = key;
        data->shader_stats[key].call_count++;
        data->shader_stats[key].workload_count += (size_t)workload;
    } else {
        auto data                    = getCurrentThreadData();
        data->current_local_registry = shader.LocalRegistry.get();
        data->current_shader_key     = key;
        data->shader_stats[key].call_count++;
        data->shader_stats[key].workload_count += (size_t)workload;
    }
}

ShaderInfo& DeviceInterface::getCurrentShader()
{
    if (isGPU())
        return mShaderInfos.at(mDeviceData.current_shader_key);
    else
        return mShaderInfos.at(getCurrentThreadData()->current_shader_key);
}

DeviceInterface::DeviceBufferProxy<float> DeviceInterface::getStream(StreamType type, size_t buffer, size_t size, size_t minComponents)
{
    const bool isPrimary = type == StreamType::Primary;
    if (mSetupSettings.DebugTrace) {
        if (isPrimary)
            IG_LOG(L_DEBUG) << "TRACE> Get Primary Streams" << std::endl;
        else
            IG_LOG(L_DEBUG) << "TRACE> Get Secondary Streams" << std::endl;
    }

    const size_t payloads = isPrimary ? mCurrentRenderSettings.info.PrimaryPayloadCount : mCurrentRenderSettings.info.SecondaryPayloadCount;
    const size_t elements = roundUp(minComponents + payloads, 4);

    const size_t offset = isPrimary ? 0 : 1;

    auto& stream = isGPU() ? *mDeviceData.current_streams[buffer + offset * GPUStreamBufferCount] : getCurrentThreadData()->streams[offset];
    resizeArray(mDeviceID, stream.Data, size, elements);
    stream.BlockSize = size;

    return {
        .DataPtr   = stream.Data.data(),
        .DataSize  = stream.Data.size() * sizeof(float),
        .BlockSize = stream.BlockSize
    };
}

DeviceInterface::DeviceBufferProxy<float> DeviceInterface::getStream(StreamType type, size_t buffer)
{
    const bool isPrimary = type == StreamType::Primary;
    if (mSetupSettings.DebugTrace) {
        if (isPrimary)
            IG_LOG(L_DEBUG) << "TRACE> Get Readonly Primary Streams" << std::endl;
        else
            IG_LOG(L_DEBUG) << "TRACE> Get Readonly Secondary Streams" << std::endl;
    }

    const size_t offset = isPrimary ? 0 : 1;

    if (isGPU()) {
        IG_ASSERT(mDeviceData.current_streams[buffer + offset * GPUStreamBufferCount]->Data.size() > 0, "Expected gpu stream to be initialized");
        auto& stream = *mDeviceData.current_streams[buffer + offset * GPUStreamBufferCount];

        return {
            .DataPtr   = stream.Data.data(),
            .DataSize  = stream.Data.size() * sizeof(float),
            .BlockSize = stream.BlockSize
        };
    } else {
        IG_ASSERT(getCurrentThreadData()->streams[offset].Data.size() > 0, "Expected cpu stream to be initialized");
        auto& stream = getCurrentThreadData()->streams[offset];

        return {
            .DataPtr   = stream.Data.data(),
            .DataSize  = stream.Data.size() * sizeof(float),
            .BlockSize = stream.BlockSize
        };
    }
}

void DeviceInterface::swapGPUStreams(StreamType type)
{
    const bool isPrimary = type == StreamType::Primary;
    if (mSetupSettings.DebugTrace) {
        if (isPrimary)
            IG_LOG(L_DEBUG) << "TRACE> Swap GPU Primary Streams" << std::endl;
        else
            IG_LOG(L_DEBUG) << "TRACE> Swap GPU Secondary Streams" << std::endl;
    }

    const size_t offset = isPrimary ? 0 : 1;
    std::swap(mDeviceData.current_streams[0 + offset * GPUStreamBufferCount], mDeviceData.current_streams[1 + offset * GPUStreamBufferCount]);
}

IDeviceInterface::TemporaryStorageHostProxy DeviceInterface::getTemporaryStorageHost()
{
    // Upper bound extracted from "mapping_*.art"
    const size_t size = roundUp(std::max<size_t>(32, std::max(mEntityCount + 1, (mCurrentSceneSettings.database->MaterialCount + 1) * 2)), 4);

    TemporaryStorageHost* tmp;
    if (!isGPU())
        tmp = &getCurrentThreadData()->temporary_storage_host;
    else
        tmp = &mDeviceData.temporary_storage_host;

    IG_ASSERT(tmp != nullptr, "Expected valid temporary storage host pointer");

    *tmp = TemporaryStorageHost{
        std::move(resizeArray(0 /*Host*/, tmp->ray_begins, size, 1)),
        std::move(resizeArray(0 /*Host*/, tmp->ray_ends, size, 1))
    };

    return {
        .RayBeginsPtr = tmp->ray_begins.data(),
        .RayEndsPtr   = tmp->ray_ends.data(),
    };
}

void* DeviceInterface::loadRayList()
{
    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Load Ray List" << std::endl;

    size_t count = mCurrentRenderSettings.width;
    auto& device = mDeviceData;
    if (device.ray_list.size() == (int64_t)count)
        return (void*)&device.ray_list;

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

    device.ray_list = copyToDevice(mDeviceID, rays);
    return (void*)&device.ray_list;
}

IDeviceInterface::DyntableProxy DeviceInterface::loadDyntable(const std::string& name)
{
    static_assert(sizeof(::LookupEntry) == sizeof(LookupEntry), "Expected generated Lookup Entry and internal Lookup Entry to be of same size!");

    std::lock_guard<std::mutex> _guard(mThreadMutex);

    DeviceDyntable* dyntable;

    auto& tables = mDeviceData.dyntables;
    auto it      = tables.find(name);
    if (it != tables.end()) {
        dyntable = &it->second;
    } else {

        IG_LOG(L_DEBUG) << "Loading dyntable '" << name << "'" << std::endl;

        const auto& tbl = mCurrentSceneSettings.database->DynTables.at(name);
        tables[name]    = {
               .EntryCount    = tbl.entryCount(),
               .LookupEntries = ShallowArray<::LookupEntry>(mDeviceID, (::LookupEntry*)tbl.lookups().data(), tbl.lookups().size()),
               .Data          = ShallowArray<uint8_t>(mDeviceID, tbl.data().data(), tbl.data().size())
        };
        dyntable = &tables[name];
    }

    IG_ASSERT(dyntable != nullptr, "Expected valid dyntable pointer");
    return {
        .EntryCount    = dyntable->EntryCount,
        .LookupEntries = (LookupEntry*)dyntable->LookupEntries.ptr(),
        .DataPtr       = const_cast<uint8*>(dyntable->Data.ptr()),
        .DataSize      = dyntable->Data.size()
    };
}

IDeviceInterface::FixtableProxy DeviceInterface::loadFixtable(const std::string& name)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    auto& tables = mDeviceData.fixtables;
    auto it      = tables.find(name);
    if (it != tables.end()) {
        return mapToProxy(it->second);
    } else {
        IG_LOG(L_DEBUG) << "Loading fixtable '" << name << "'" << std::endl;
        IG_ASSERT(mCurrentSceneSettings.database->FixTables.count(name) > 0, "Expected given fixtable name to be available");

        tables[name] = DeviceBuffer{ copyToDevice(mDeviceID, mCurrentSceneSettings.database->FixTables.at(name).data()), 1 };
        return mapToProxy(tables.at(name));
    }
}

void DeviceInterface::loadEntityBVH(BVHType type, const char* prim_type, void** nodes, void** objs)
{
    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Load Entity BVH" << std::endl;

    std::lock_guard<std::mutex> _guard(mThreadMutex);

    auto& device    = mDeviceData;
    std::string str = prim_type;
    auto it         = device.bvh_ents.find(str);
    if (it == device.bvh_ents.end()) {
        IG_LOG(L_DEBUG) << "Loading scene bvh " << prim_type << std::endl;
        const auto& bvh         = mCurrentSceneSettings.database->SceneBVHs.at(prim_type);
        const size_t leaf_count = bvh.Leaves.size() / sizeof(EntityLeaf1);

        switch (type) {
        default:
        case BVHType::BVH2: {
            const size_t node_count = bvh.Nodes.size() / sizeof(Node2);
            device.bvh_ents[str]    = IG::Bvh2Ent{
                std::move(ShallowArray<Node2>(mDeviceID, reinterpret_cast<const Node2*>(bvh.Nodes.data()), node_count)),
                std::move(ShallowArray<EntityLeaf1>(mDeviceID, reinterpret_cast<const EntityLeaf1*>(bvh.Leaves.data()), leaf_count))
            };
        } break;
        case BVHType::BVH4: {
            const size_t node_count = bvh.Nodes.size() / sizeof(Node4);
            device.bvh_ents[str]    = IG::Bvh4Ent{
                std::move(ShallowArray<Node4>(mDeviceID, reinterpret_cast<const Node4*>(bvh.Nodes.data()), node_count)),
                std::move(ShallowArray<EntityLeaf1>(mDeviceID, reinterpret_cast<const EntityLeaf1*>(bvh.Leaves.data()), leaf_count))
            };
        } break;
        case BVHType::BVH8: {
            const size_t node_count = bvh.Nodes.size() / sizeof(Node8);
            device.bvh_ents[str]    = IG::Bvh8Ent{
                std::move(ShallowArray<Node8>(mDeviceID, reinterpret_cast<const Node8*>(bvh.Nodes.data()), node_count)),
                std::move(ShallowArray<EntityLeaf1>(mDeviceID, reinterpret_cast<const EntityLeaf1*>(bvh.Leaves.data()), leaf_count))
            };
        } break;
        }

        it = device.bvh_ents.find(str);
    }

    IG_ASSERT(it != device.bvh_ents.end(), "Expected valid iterator for scene bvh");

    switch (type) {
    default:
    case BVHType::BVH2: {
        auto& bvh = std::get<IG::Bvh2Ent>(it->second);
        *nodes    = const_cast<Node2*>(bvh.Nodes.ptr());
        *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
    } break;
    case BVHType::BVH4: {
        auto& bvh = std::get<IG::Bvh4Ent>(it->second);
        *nodes    = const_cast<Node4*>(bvh.Nodes.ptr());
        *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
    } break;
    case BVHType::BVH8: {
        auto& bvh = std::get<IG::Bvh8Ent>(it->second);
        *nodes    = const_cast<Node8*>(bvh.Nodes.ptr());
        *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
    } break;
    }
}

IDeviceInterface::DeviceImageProxy<float> DeviceInterface::loadImageFromFile(const std::string& filename, int32_t expected_channels)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);
    DeviceGuard _threadGuard(this);

    auto& images = mDeviceData.images;
    auto it      = images.find(filename);
    if (it != images.end())
        return mapToProxy(it->second);

    _SECTION(SectionType::ImageLoading);

    IG_LOG(L_DEBUG) << "Loading image '" << filename << "' (C=" << expected_channels << ")" << std::endl;
    try {
        const auto img = Image::load(filename);
        if (expected_channels != (int32_t)img.channels) {
            IG_LOG(L_ERROR) << "Image '" << filename << "' is has unexpected channel count" << std::endl;
            images[filename] = copyToDevice(mDeviceID, Image());
        } else {
            auto& res = getCurrentShader().images[filename]; // Get or construct resource info for given resource
            res.counter++;
            res.memory_usage = img.width * img.height * img.channels * sizeof(float);
            images[filename] = copyToDevice(mDeviceID, img);
        }
    } catch (const ImageLoadException& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        images[filename] = copyToDevice(mDeviceID, MissingImage);
    }
    return mapToProxy(images.at(filename));
}

IDeviceInterface::DeviceImageProxy<uint8_t> DeviceInterface::loadPackedImageFromFile(const std::string& filename, int32_t expected_channels, bool linear)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);
    DeviceGuard _threadGuard(this);

    auto& images = mDeviceData.packed_images;
    auto it      = images.find(filename);
    if (it != images.end())
        return mapToProxy(it->second);

    _SECTION(SectionType::PackedImageLoading);

    IG_LOG(L_DEBUG) << "Loading (packed) image '" << filename << "' (C=" << expected_channels << ")" << std::endl;
    try {
        std::vector<uint8_t> packed;
        size_t width, height, channels;
        Image::loadAsPacked(filename, packed, width, height, channels, linear);

        if (expected_channels != (int32_t)channels) {
            IG_LOG(L_ERROR) << "Packed image '" << filename << "' is has unexpected channel count" << std::endl;
            images[filename] = copyToDevicePacked(mDeviceID, MissingImage);
        } else {
            auto& res = getCurrentShader().packed_images[filename]; // Get or construct resource info for given resource
            res.counter++;
            res.memory_usage = packed.size();
            images[filename] = DevicePackedImage{ copyToDevice(mDeviceID, packed), width, height };
        }
    } catch (const ImageLoadException& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        images[filename] = copyToDevicePacked(mDeviceID, MissingImage);
    }
    return mapToProxy(images.at(filename));
}

static std::vector<uint8_t> readBufferFile(const std::string& filename)
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

IDeviceInterface::DeviceBufferProxy<uint8_t> DeviceInterface::loadBufferFromFile(const std::string& filename)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);
    DeviceGuard _threadGuard(this);

    auto& buffers = mDeviceData.buffers;
    auto it       = buffers.find(filename);
    if (it != buffers.end())
        return mapToProxy(it->second);

    _SECTION(SectionType::BufferLoading);

    IG_LOG(L_DEBUG) << "Loading buffer '" << filename << "'" << std::endl;
    const auto vec = readBufferFile(filename);

    if ((vec.size() % sizeof(int32_t)) != 0)
        IG_LOG(L_WARNING) << "Buffer '" << filename << "' is not properly sized!" << std::endl;

    buffers[filename] = DeviceBuffer{ copyToDevice(mDeviceID, vec), 1 };

    return mapToProxy(buffers.at(filename));
}

IDeviceInterface::DeviceBufferProxy<uint8_t> DeviceInterface::loadBufferByName(const std::string& name)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    auto& buffers = mDeviceData.buffers;
    auto it       = buffers.find(name);
    if (it != buffers.end())
        return mapToProxy(it->second);

    IG_LOG(L_ERROR) << "No buffer '" << name << "'" << std::endl;

    return {
        .DataPtr   = nullptr,
        .DataSize  = 0,
        .BlockSize = 0
    };
}

IDeviceInterface::DeviceBufferProxy<uint8_t> DeviceInterface::requestBuffer(const std::string& name, int32_t size, int32_t flags)
{
    IG_UNUSED(flags); // We do not make use of it yet

    std::lock_guard<std::mutex> _guard(mThreadMutex);
    DeviceGuard _threadGuard(this);

    IG_ASSERT(size > 0, "Expected buffer size to be larger then zero");

    // Make sure the buffer is properly sized
    size = (int32_t)roundUp(size, 32);

    auto& buffers = mDeviceData.buffers;
    if (auto it = buffers.find(name); it != buffers.end() && it->second.Data.size() >= (int64_t)size)
        return mapToProxy(it->second);

    _SECTION(SectionType::BufferRequests);

    IG_LOG(L_DEBUG) << "Requested buffer '" << name << "' with " << FormatMemory(size) << std::endl;

    void* ptr = anydsl_alloc(mDeviceID, size);
    if (ptr == nullptr) {
        IG_LOG(L_FATAL) << "Out of memory" << std::endl;
        std::abort();
    }

    buffers[name] = DeviceBuffer{ anydsl::Array<uint8_t>(mDeviceID, reinterpret_cast<uint8_t*>(ptr), size), 1 };
    return mapToProxy(buffers.at(name));
}

void DeviceInterface::saveBuffer(const std::string& name, const std::string& filename)
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

bool DeviceInterface::copyBufferToHost(const std::string& buffer_name, void* dst, size_t maxSizeByte)
{
    auto& buffers = mDeviceData.buffers;
    uint8* ptr    = nullptr;
    size_t size   = 0;
    if (auto it = buffers.find(buffer_name); it != buffers.end()) {
        ptr  = it->second.Data.data();
        size = (size_t)it->second.Data.size();
    }

    if (ptr == nullptr)
        return false;

    size = std::min(size, maxSizeByte);
    if (size == 0)
        return false;

    anydsl_copy(mDeviceID, ptr, 0, 0 /* Host */, dst, 0, size);

    return true;
}

void DeviceInterface::handleDebugOutput()
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

    if (const auto it = mDeviceData.buffers.find("__dbg_output"); it != mDeviceData.buffers.end()) {
        DeviceBuffer& buffer = it->second;
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
}

IDeviceInterface::DeviceImageProxy<float> DeviceInterface::loadAOVImageForCPU(const std::string& aov_name)
{
    IG_ASSERT(!isGPU(), "Should only be called if not GPU");

    if (aov_name.empty() || aov_name == DefaultFramebufferName)
        return getFramebuffer();

    const auto it = mAOVs.find(aov_name);
    if (it == mAOVs.end()) {
        IG_LOG(L_ERROR) << "Unknown aov '" << aov_name << "' access" << std::endl;
        return DeviceImageProxy<float>::Invalid();
    }

    return { .DataPtr = it->second.Data.data(), .Width = mFramebufferWidth, .Height = mFramebufferHeight };
}

IDeviceInterface::DeviceImageProxy<float> DeviceInterface::loadAOVImageForDevice(const std::string& aov_name)
{
    if (!mHostFramebuffer.Data.data()) {
        IG_LOG(L_ERROR) << "Framebuffer not yet initialized. Run a single iteration first" << std::endl;
        return DeviceImageProxy<float>::Invalid();
    }

    if (isGPU()) {
        if (aov_name.empty() || aov_name == DefaultFramebufferName) {
            return { .DataPtr = mDeviceData.film_pixels.data(), .Width = mFramebufferWidth, .Height = mFramebufferHeight };
        } else {
            if (const auto it = mAOVs.find(aov_name); it != mAOVs.end()) {
                return { .DataPtr = ensurePresentOnDevice(mDeviceID, mDeviceData.aovs[aov_name], mAOVs[aov_name].Data).data(),
                         .Width   = mFramebufferWidth,
                         .Height  = mFramebufferHeight };
            } else {
                IG_LOG(L_ERROR) << "Unknown aov '" << aov_name << "' access" << std::endl;
                return DeviceImageProxy<float>::Invalid();
            }
        }
    } else {
        return loadAOVImageForCPU(aov_name);
    }
}

IDeviceInterface::DeviceImageProxy<float> DeviceInterface::loadAOVImageForHost(const std::string& aov_name)
{
    if (!mHostFramebuffer.Data.data()) {
        IG_LOG(L_ERROR) << "Framebuffer not yet initialized. Run a single iteration first" << std::endl;
        return DeviceImageProxy<float>::Invalid();
    }

    DeviceGuard _guard(this);

    if (isGPU()) {
        if (aov_name.empty() || aov_name == DefaultFramebufferName) {
            if (mHostFramebuffer.Dirty && mDeviceData.film_pixels.data() != nullptr) {
                _SECTION(SectionType::FramebufferHostUpdate);
                anydsl::copy(mDeviceData.film_pixels, mHostFramebuffer.Data);
                mHostFramebuffer.Dirty = false;
            }
            return { .DataPtr = mHostFramebuffer.Data.data(), .Width = mFramebufferWidth, .Height = mFramebufferHeight };
        } else {
            const auto it = mAOVs.find(aov_name);
            if (it == mAOVs.end()) {
                IG_LOG(L_ERROR) << "Unknown aov '" << aov_name << "' access for host" << std::endl;
                return DeviceImageProxy<float>::Invalid();
            }

            if (it->second.Dirty && mDeviceData.aovs[aov_name].data() != nullptr) {
                _SECTION(SectionType::AOVHostUpdate);
                anydsl::copy(mDeviceData.aovs[aov_name], it->second.Data);
                it->second.Dirty = false;
            }
            return { .DataPtr = it->second.Data.data(), .Width = mFramebufferWidth, .Height = mFramebufferHeight };
        }
    } else {
        return loadAOVImageForCPU(aov_name);
    }
}

void DeviceInterface::mapAOVBackToDevice(const std::string& aov_name)
{
    if (!isGPU()) // Device is host
        return;

    DeviceGuard _guard(this);

    if (aov_name.empty() || aov_name == DefaultFramebufferName) {
        _SECTION(SectionType::FramebufferUpdate);
        anydsl::copy(mHostFramebuffer.Data, mDeviceData.film_pixels);
        mHostFramebuffer.Dirty = false;
    } else {
        if (const auto it = mAOVs.find(aov_name); it != mAOVs.end()) {
            if (mDeviceData.aovs[aov_name].size() != it->second.Data.size()) {
                _SECTION(SectionType::AOVUpdate);
                mDeviceData.aovs[aov_name] = createFramebuffer(mDeviceID);
            }

            anydsl::copy(it->second.Data, mDeviceData.aovs[aov_name]);
            it->second.Dirty = false;
        } else {
            IG_LOG(L_ERROR) << "Unknown aov '" << aov_name << "' mapping" << std::endl;
        }
    }
}

void DeviceInterface::mapAllAOVsBackToDevice()
{
    mapAOVBackToDevice({});
    for (const auto& p : mAOVs)
        mapAOVBackToDevice(p.first);
}

/// Clear specific aov
void DeviceInterface::clearAOV(const std::string& aov_name)
{
    if (!mHostFramebuffer.Data.data())
        return;

    if (aov_name.empty() || aov_name == DefaultFramebufferName) {
        mHostFramebuffer.Dirty = true;
        std::memset(mHostFramebuffer.Data.data(), 0, sizeof(float) * mHostFramebuffer.Data.size());
        if (mDeviceData.film_pixels.size() == mHostFramebuffer.Data.size())
            anydsl::copy(mHostFramebuffer.Data, mDeviceData.film_pixels);
    } else {
        auto& aov    = mAOVs.at(aov_name);
        aov.Dirty    = true;
        auto& buffer = aov.Data;
        std::memset(buffer.data(), 0, sizeof(float) * buffer.size());
        if (const auto it = mDeviceData.aovs.find(aov_name); it != mDeviceData.aovs.end()) {
            if (it->second.size() == buffer.size())
                anydsl::copy(buffer, it->second);
        }
    }
}

/// Clear all aovs and the framebuffer
void DeviceInterface::clearAllAOVs()
{
    clearAOV({});
    for (const auto& p : mAOVs)
        clearAOV(p.first.c_str());
}

// -------------------------------------------------------- Shader
void DeviceInterface::runDeviceShader()
{
    DeviceGuard _guard(this);
    ensureFramebuffer();

    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Device Shader " << mSetupSettings.Target.toString() << std::endl;

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.beginShaderLaunch(ShaderType::Device, 1, {});

    using Callback = decltype(ig_callback_shader);
    auto callback  = reinterpret_cast<Callback*>(mCurrentShaderSet.DeviceShader.Exec);
    IG_ASSERT(callback != nullptr, "Expected device shader to be valid");
    setCurrentShader(1, ShaderKey(mCurrentShaderSet.ID, ShaderType::Device, 0), mCurrentShaderSet.DeviceShader);
    callback(&mCurrentDriverSettings);

    handleDebugOutput();

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.endShaderLaunch(ShaderType::Device, {});
}

void DeviceInterface::runTonemapShader(float* in_pixels, uint32_t* device_out_pixels, const TonemapSettings& settings)
{
    DeviceGuard _guard(this);
    ensureFramebuffer();

    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Tonemap Shader" << std::endl;

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.beginShaderLaunch(ShaderType::Tonemap, 1, {});

    using Callback = decltype(ig_tonemap_shader);
    auto callback  = reinterpret_cast<Callback*>(mCurrentShaderSet.TonemapShader.Exec);
    IG_ASSERT(callback != nullptr, "Expected tonemap shader to be valid");
    setCurrentShader(1, ShaderKey(mCurrentShaderSet.ID, ShaderType::Tonemap, 0), mCurrentShaderSet.TonemapShader);

    ::TonemapSettings driver_settings;
    driver_settings.method          = (int)settings.Method;
    driver_settings.use_gamma       = settings.UseGamma;
    driver_settings.scale           = settings.Scale;
    driver_settings.exposure_factor = settings.ExposureFactor;
    driver_settings.exposure_offset = settings.ExposureOffset;
    callback(&mCurrentDriverSettings, in_pixels, device_out_pixels, (int)mFramebufferWidth, (int)mFramebufferHeight, &driver_settings);

    handleDebugOutput();

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.endShaderLaunch(ShaderType::Tonemap, {});
}

ImageInfoOutput DeviceInterface::runImageInfoShader(float* in_pixels, const ImageInfoSettings& settings)
{
    DeviceGuard _guard(this);
    ensureFramebuffer();

    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Imageinfo Shader" << std::endl;

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.beginShaderLaunch(ShaderType::ImageInfo, 1, {});

    using Callback = decltype(ig_imageinfo_shader);
    auto callback  = reinterpret_cast<Callback*>(mCurrentShaderSet.ImageinfoShader.Exec);
    IG_ASSERT(callback != nullptr, "Expected imageinfo shader to be valid");
    setCurrentShader(1, ShaderKey(mCurrentShaderSet.ID, ShaderType::ImageInfo, 0), mCurrentShaderSet.ImageinfoShader);

    ::ImageInfoSettings driver_settings;
    driver_settings.scale               = settings.Scale;
    driver_settings.bins                = (int)settings.Bins;
    driver_settings.histogram_r         = settings.HistogramR;
    driver_settings.histogram_g         = settings.HistogramG;
    driver_settings.histogram_b         = settings.HistogramB;
    driver_settings.histogram_l         = settings.HistogramL;
    driver_settings.acquire_error_stats = settings.AcquireErrorStats;
    driver_settings.acquire_histogram   = settings.AcquireHistogram;

    ::ImageInfoOutput driver_output;
    callback(&mCurrentDriverSettings, in_pixels, (int)mFramebufferWidth, (int)mFramebufferHeight, &driver_settings, &driver_output);

    handleDebugOutput();

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.endShaderLaunch(ShaderType::ImageInfo, {});

    IG::ImageInfoOutput output;
    output.Min      = driver_output.min;
    output.Max      = driver_output.max;
    output.Average  = driver_output.avg;
    output.SoftMin  = driver_output.soft_min;
    output.SoftMax  = driver_output.soft_max;
    output.Median   = driver_output.median;
    output.InfCount = driver_output.inf_counter;
    output.NaNCount = driver_output.nan_counter;
    output.NegCount = driver_output.neg_counter;

    return output;
}

void DeviceInterface::runTraversalShader(TraversalStage stage, int size)
{
    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Traversal Shader [T=" << (int)stage << ", S=" << size << "]" << std::endl;

    const ShaderType shaderType = stage == TraversalStage::Primary ? ShaderType::PrimaryTraversal : ShaderType::SecondaryTraversal;

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.beginShaderLaunch(shaderType, size, {});

    auto& shader = stage == TraversalStage::Primary ? mCurrentShaderSet.PrimaryTraversalShader : mCurrentShaderSet.SecondaryTraversalShader;

    using Callback = decltype(ig_traversal_shader);
    auto callback  = reinterpret_cast<Callback*>(shader.Exec);
    IG_ASSERT(callback != nullptr, "Expected traversal shader to be valid");
    setCurrentShader(size, ShaderKey(mCurrentShaderSet.ID, shaderType, 0), shader);
    callback(&mCurrentDriverSettings, size);

    handleDebugOutput();

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.endShaderLaunch(shaderType, {});
}

int DeviceInterface::runRayGenerationShader(int next_id, int size, int xmin, int ymin, int xmax, int ymax)
{
    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Ray Generation Shader [S=" << size << ", I=" << next_id << "]" << std::endl;

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.beginShaderLaunch(ShaderType::RayGeneration, (xmax - xmin) * (ymax - ymin), {});

    using Callback = decltype(ig_ray_generation_shader);
    auto callback  = reinterpret_cast<Callback*>(mCurrentShaderSet.RayGenerationShader.Exec);
    IG_ASSERT(callback != nullptr, "Expected ray generation shader to be valid");
    setCurrentShader((xmax - xmin) * (ymax - ymin), ShaderKey(mCurrentShaderSet.ID, ShaderType::RayGeneration, 0), mCurrentShaderSet.RayGenerationShader);
    const int ret = callback(&mCurrentDriverSettings, next_id, size, xmin, ymin, xmax, ymax);

    handleDebugOutput();

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.endShaderLaunch(ShaderType::RayGeneration, {});

    return ret;
}

void DeviceInterface::runMaterialShader(int material_id, int first, int last)
{
    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Material Shader [M=" << material_id << ", S=" << first << ", E=" << last << "]" << std::endl;

    const ShaderType shaderType = material_id >= 0 ? ShaderType::Hit : ShaderType::Miss;

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.beginShaderLaunch(shaderType, last - first, material_id);

    using Callback     = decltype(ig_material_shader);
    const auto& output = material_id >= 0 ? mCurrentShaderSet.HitShaders.at(material_id) : mCurrentShaderSet.MissShader;
    auto callback      = reinterpret_cast<Callback*>(output.Exec);
    IG_ASSERT(callback != nullptr, "Expected hit shader to be valid");
    setCurrentShader(last - first, ShaderKey(mCurrentShaderSet.ID, shaderType, (uint32)material_id), output);
    callback(&mCurrentDriverSettings, material_id, first, last);

    handleDebugOutput();

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.endShaderLaunch(shaderType, material_id);
}

void DeviceInterface::runAdvancedShadowShader(int material_id, int first, int last, bool is_hit)
{
    IG_ASSERT(!mCurrentShaderSet.AdvancedShadowHitShaders.empty() && !mCurrentShaderSet.AdvancedShadowMissShaders.empty(), "Expected advanced shadow shader only be called if it is enabled!");

    const ShaderType shaderType = is_hit ? ShaderType::AdvancedShadowHit : ShaderType::AdvancedShadowMiss;

    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Advanced Shadow Shader [I=" << material_id << ", S=" << first << ", E=" << last << ", Hit=" << (is_hit ? "true" : "false") << "]" << std::endl;

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.beginShaderLaunch(shaderType, last - first, material_id);

    using Callback = decltype(ig_advanced_shadow_shader);

    const auto& outputs = is_hit ? mCurrentShaderSet.AdvancedShadowHitShaders : mCurrentShaderSet.AdvancedShadowMissShaders;

    IG_ASSERT(material_id >= 0 && material_id < (int)outputs.size(), "Expected material id for advanced shadow shaders to be valid");
    const auto& output = outputs.at(material_id);
    auto callback      = reinterpret_cast<Callback*>(output.Exec);
    IG_ASSERT(callback != nullptr, "Expected advanced shadow shader to be valid");
    setCurrentShader(last - first, ShaderKey(mCurrentShaderSet.ID, shaderType, (uint32)material_id), output);
    callback(&mCurrentDriverSettings, material_id, first, last);

    handleDebugOutput();

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.endShaderLaunch(shaderType, material_id);
}

void DeviceInterface::runCallbackShader(int type)
{
    DeviceGuard _guard(this);
    ensureFramebuffer();

    IG_ASSERT(type >= 0 && type < (int)CallbackType::_COUNT, "Expected callback shader type to be well formed!");

    const auto& output = mCurrentShaderSet.CallbackShaders[type];
    using Callback     = decltype(ig_callback_shader);
    auto callback      = reinterpret_cast<Callback*>(output.Exec);

    if (callback != nullptr) {
        if (mSetupSettings.DebugTrace)
            IG_LOG(L_DEBUG) << "TRACE> Callback Shader [T=" << type << "]" << std::endl;

        if (mSetupSettings.AcquireStats)
            getCurrentThreadData()->stats.beginShaderLaunch(ShaderType::Callback, 1, type);

        setCurrentShader(1, ShaderKey(mCurrentShaderSet.ID, ShaderType::Callback, (uint32)type), output);
        callback(&mCurrentDriverSettings);

        handleDebugOutput();

        if (mSetupSettings.AcquireStats)
            getCurrentThreadData()->stats.endShaderLaunch(ShaderType::Callback, type);
    }
}

void DeviceInterface::runBakeShader(const ShaderOutput<void*>& shader, const std::vector<std::string>* resource_map, float* output)
{
    DeviceGuard _guard(this);
    // No access to the framebuffer!

    IG_ASSERT(shader.Exec != nullptr, "Expected bake shader to be valid");

    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Bake Shader" << std::endl;

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.beginShaderLaunch(ShaderType::Bake, 1, {});

    const auto copy                    = mCurrentSceneSettings.resource_map;
    mCurrentSceneSettings.resource_map = resource_map;

    using Callback = decltype(ig_bake_shader);
    auto callback  = reinterpret_cast<Callback*>(shader.Exec);

    setCurrentShader(1, ShaderKey(0, ShaderType::Bake, 0), shader);
    callback(&mCurrentDriverSettings, output);

    handleDebugOutput();

    mCurrentSceneSettings.resource_map = copy;

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.endShaderLaunch(ShaderType::Bake, {});
}

void DeviceInterface::runPassShader(const ShaderOutput<void*>& shader, void* userData)
{
    DeviceGuard _guard(this);
    ensureFramebuffer();

    IG_ASSERT(shader.Exec != nullptr, "Expected pass shader to be valid");

    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Pass Shader" << std::endl;

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.beginShaderLaunch(ShaderType::Pass, 1, {});

    using Callback = decltype(ig_pass_main);
    auto callback  = reinterpret_cast<Callback*>(shader.Exec);

    setCurrentShader(1, ShaderKey(0, ShaderType::Pass, 0), shader);
    callback(&mCurrentDriverSettings, (int32*)userData);

    handleDebugOutput();

    if (mSetupSettings.AcquireStats)
        getCurrentThreadData()->stats.endShaderLaunch(ShaderType::Pass, {});
}

void DeviceInterface::beginStatsSection(int id)
{
    DeviceGuard _guard(this);
    getCurrentThreadData()->stats.beginSection((IG::SectionType)id);
}

void DeviceInterface::endStatsSection(int id)
{
    DeviceGuard _guard(this);
    getCurrentThreadData()->stats.endSection((IG::SectionType)id);
}

void DeviceInterface::addStatsValue(int id, int value)
{
    DeviceGuard _guard(this);
    getCurrentThreadData()->stats.increase((IG::Quantity)id, static_cast<uint64_t>(value));
}

void DeviceInterface::updateStatistics()
{
    mAcquiredStats.reset();
    for (const auto& data : mThreadData)
        mAcquiredStats.add(data->stats);
}

const Statistics& DeviceInterface::getAcquiredStatistics()
{
    updateStatistics();
    return mAcquiredStats;
}

void DeviceInterface::enterDevice()
{
    enableFastMathModeForThread();
    registerThread();
}

void DeviceInterface::leaveDevice()
{
    unregisterThread();
    disableFastMathModeForThread();
}

} // namespace IG