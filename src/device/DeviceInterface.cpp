#include "DeviceInterface.h"
#include "Image.h"
#include "Logger.h"
#include "RuntimeStructs.h"
#include "device/DeviceUtils.h"
#include "table/SceneDatabase.h"

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

#define _SECTION(type)                                                         \
    const auto sectionClosure = getCurrentThreadData()->stats.section((type)); \
    IG_UNUSED(sectionClosure)

namespace IG {
static const std::string DefaultFramebufferName = "Color";
static inline bool checkIfAOVIsFramebuffer(const std::string& aov_name)
{
    return aov_name.empty() || aov_name == DefaultFramebufferName;
}

static inline const std::string& handleAOVName(const std::string& aov_name)
{
    if (aov_name.empty())
        return DefaultFramebufferName;
    else
        return aov_name;
}

static inline size_t roundUp(size_t num, size_t multiple)
{
    if (multiple == 0)
        return num;

    size_t remainder = num % multiple;
    if (remainder == 0)
        return num;

    return num + multiple - remainder;
}

template <typename T>
static inline void resizeUnifiedArray(int32_t dev, UnifiedArray<T>& array, size_t size, size_t multiplier)
{
    const auto capacity = roundUp(size, 32);
    const size_t n      = capacity * multiplier;
    if (array.SizeInBytes < n * sizeof(T))
        array = UnifiedArray<T>::AllocateUnified(dev, n);
}

template <typename T>
static inline void resizeDeviceArray(int32_t dev, UnifiedArray<T>& array, size_t size, size_t multiplier)
{
    const auto capacity = roundUp(size, 32);
    const size_t n      = capacity * multiplier;
    if (array.SizeInBytes < n * sizeof(T))
        array = UnifiedArray<T>::AllocateDevice(dev, n);
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
inline IDeviceInterface::DeviceBufferProxy<T> mapToProxy(const DeviceBufferBase<T>& buffer)
{
    return IDeviceInterface::DeviceBufferProxy<T>{
        .DataPtr     = const_cast<T*>(buffer.Data.DevicePtr),
        .SizeInBytes = buffer.Data.SizeInBytes
    };
}

template <typename T>
inline IDeviceInterface::DeviceImageProxy<T> mapToProxy(const DeviceImageBase<T>& buffer)
{
    return IDeviceInterface::DeviceImageProxy<T>{
        .DataPtr = const_cast<T*>(buffer.Data.DevicePtr),
        .Width   = buffer.Width,
        .Height  = buffer.Height
    };
}

template <typename T>
inline IDeviceInterface::DeviceStreamProxy<T> mapToProxy(const DeviceStreamBase<T>& buffer)
{
    return IDeviceInterface::DeviceStreamProxy<T>{
        .DataPtr   = const_cast<T*>(buffer.Data.DevicePtr),
        .BlockSize = buffer.BlockSize
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

void DeviceInterface::connectGlobalRegistry(ParameterSet* parameter_set)
{
    mCurrentParameters = parameter_set;
}

void DeviceInterface::setCurrentSceneSettings(const Device::SceneSettings& settings)
{
    mCurrentSceneSettings = settings;
    mEntityCount          = mCurrentSceneSettings.database->FixTables.count("entities") > 0 ? mCurrentSceneSettings.database->FixTables.at("entities").entryCount() : 0;
}

std::vector<std::string> DeviceInterface::getAOVNames() const
{
    std::vector<std::string> names;
    names.reserve(mDeviceData.aovs.size());
    for (const auto& p : mDeviceData.aovs)
        names.push_back(p.first);

    return names;
}

IDeviceInterface::DeviceImageProxy<float> DeviceInterface::getFramebufferForDevice()
{
    return loadAOVImageForDevice(DefaultFramebufferName);
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
    std::lock_guard<std::mutex> _guard(mThreadMutex);
    const size_t expectedSize = framebufferArea() * 3;

    IG_ASSERT(expectedSize > 0, "Expected host framebuffer to have a valid size");
    if (const auto it = mDeviceData.aovs.find(DefaultFramebufferName); it != mDeviceData.aovs.end()) {
        // Check if resize is needed
        if (it->second.Width != mFramebufferWidth || it->second.Height != mFramebufferHeight) {
            // Update properties
            for (auto& p : mDeviceData.aovs) {
                p.second.Width  = mFramebufferWidth;
                p.second.Height = mFramebufferHeight;
            }

            // Resize if needed
            if (it->second.Data.SizeInBytes < expectedSize * sizeof(float)) {
                for (auto& p : mDeviceData.aovs) {
                    p.second.Data = UnifiedArray<float>::AllocateUnified(mDeviceID, expectedSize);
                    p.second.Data.fillWithZero();
                }
            }
        }
    } else {
        mDeviceData.aovs.emplace(DefaultFramebufferName,
                                 DeviceImage{
                                     .Data   = UnifiedArray<float>::AllocateUnified(mDeviceID, expectedSize),
                                     .Width  = mFramebufferWidth,
                                     .Height = mFramebufferHeight })
            .first->second.Data.fillWithZero();
    }
}

void DeviceInterface::markFramebufferDirty()
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);
    for (auto& p : mDeviceData.aovs)
        p.second.Data.markDirtyOnDevice();
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
        CPUData* ptr = tlThreadData;
        tlThreadData = nullptr;
        mAvailableThreadData.push(ptr);
    }
}

ParameterSet* DeviceInterface::getCurrentGlobalRegistry() { return mCurrentParameters; }

ParameterSet* DeviceInterface::getCurrentLocalRegistry()
{
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

DeviceInterface::DeviceStreamProxy<float> DeviceInterface::getStream(StreamType type, size_t buffer, size_t size, size_t minComponents)
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
    resizeDeviceArray(mDeviceID, stream.Data, size, elements);
    stream.BlockSize = size;

    return {
        .DataPtr   = stream.Data.DevicePtr,
        .BlockSize = stream.BlockSize
    };
}

DeviceInterface::DeviceStreamProxy<float> DeviceInterface::getStream(StreamType type, size_t buffer)
{
    const bool isPrimary = type == StreamType::Primary;
    if (mSetupSettings.DebugTrace) {
        if (isPrimary)
            IG_LOG(L_DEBUG) << "TRACE> Get Readonly Primary Streams" << std::endl;
        else
            IG_LOG(L_DEBUG) << "TRACE> Get Readonly Secondary Streams" << std::endl;
    }

    const size_t offset = isPrimary ? 0 : 1;
    auto& stream        = isGPU() ? *mDeviceData.current_streams[buffer + offset * GPUStreamBufferCount] : getCurrentThreadData()->streams[offset];

    IG_ASSERT(stream.Data.SizeInBytes > 0, "Expected stream to be initialized");
    return {
        .DataPtr   = stream.Data.DevicePtr,
        .BlockSize = stream.BlockSize
    };
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

    resizeDeviceArray(0 /*Host*/, tmp->ray_begins, size, 1);
    resizeDeviceArray(0 /*Host*/, tmp->ray_ends, size, 1);

    return {
        .RayBeginsPtr = tmp->ray_begins.DevicePtr,
        .RayEndsPtr   = tmp->ray_ends.DevicePtr,
    };
}

void* DeviceInterface::loadRayList()
{
    if (mSetupSettings.DebugTrace)
        IG_LOG(L_DEBUG) << "TRACE> Load Ray List" << std::endl;

    size_t count = mCurrentRenderSettings.width;
    auto& device = mDeviceData;
    if (device.ray_list.SizeInBytes == count * sizeof(StreamRay))
        return device.ray_list.DevicePtr;

    IG_ASSERT(mCurrentRenderSettings.rays != nullptr, "Expected list of rays to be available");

    device.ray_list = UnifiedArray<StreamRay>::AllocateDevice(mDeviceID, count);

    std::vector<StreamRay> host_data(isGPU() ? count : 0);
    StreamRay* ptr = isGPU() ? host_data.data() : device.ray_list.DevicePtr;

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

        ptr[i] = ray;
    }

    if (isGPU())
        device.ray_list.copyFromDeviceToExternalHost(host_data.data());

    return device.ray_list.DevicePtr;
}

IDeviceInterface::DynTableProxy DeviceInterface::loadDynTable(const std::string& name)
{
    static_assert(sizeof(::LookupEntry) == sizeof(LookupEntry), "Expected generated Lookup Entry and internal Lookup Entry to be of same size!");

    std::lock_guard<std::mutex> _guard(mThreadMutex);

    if (isGPU()) {
        DeviceDynTable* dyntable;

        auto& tables = mDeviceData.dyntables;
        if (const auto it = tables.find(name); it != tables.end()) {
            dyntable = &it->second;
        } else {
            IG_LOG(L_DEBUG) << "Loading dyntable '" << name << "'" << std::endl;

            const auto& tbl = mCurrentSceneSettings.database->DynTables.at(name);

            DeviceDynTable entry = DeviceDynTable{
                .EntryCount    = tbl.entryCount(),
                .LookupEntries = UnifiedArray<::LookupEntry>::AllocateDevice(mDeviceID, tbl.lookups().size()),
                .Data          = UnifiedArray<uint8_t>::AllocateDevice(mDeviceID, tbl.data().size())
            };

            entry.LookupEntries.copyFromExternalHostToDevice((const ::LookupEntry*)tbl.lookups().data());
            entry.Data.copyFromExternalHostToDevice(tbl.data().data());

            dyntable = &tables.emplace(name, std::move(entry)).first->second;
        }

        IG_ASSERT(dyntable != nullptr, "Expected valid dyntable pointer");
        return {
            .EntryCount    = dyntable->EntryCount,
            .LookupEntries = (LookupEntry*)dyntable->LookupEntries.DevicePtr,
            .DataPtr       = const_cast<uint8*>(dyntable->Data.DevicePtr),
            .DataSize      = dyntable->Data.SizeInBytes
        };
    } else {
        const auto& tbl = mCurrentSceneSettings.database->DynTables.at(name);
        return {
            .EntryCount    = tbl.entryCount(),
            .LookupEntries = (LookupEntry*)tbl.lookups().data(),
            .DataPtr       = const_cast<uint8*>(tbl.data().data()),
            .DataSize      = tbl.data().size() * sizeof(uint8_t)
        };
    }
}

IDeviceInterface::FixTableProxy DeviceInterface::loadFixTable(const std::string& name)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    if (isGPU()) {
        auto& tables = mDeviceData.fixtables;
        if (const auto it = tables.find(name); it != tables.end()) {
            return mapToProxy(it->second);
        } else {
            IG_LOG(L_DEBUG) << "Loading fixtable '" << name << "'" << std::endl;
            IG_ASSERT(mCurrentSceneSettings.database->FixTables.count(name) > 0, "Expected given fixtable name to be available");
            const auto& fixtable = mCurrentSceneSettings.database->FixTables.at(name);

            DeviceBuffer buffer = DeviceBuffer{ .Data = UnifiedArray<uint8>::AllocateDevice(mDeviceID, fixtable.data().size()) };
            buffer.Data.copyFromExternalHostToDevice(fixtable.data().data());
            return mapToProxy(tables.emplace(name, std::move(buffer)).first->second);
        }
    } else {
        IG_ASSERT(mCurrentSceneSettings.database->FixTables.count(name) > 0, "Expected given fixtable name to be available");
        const auto& fixtable = mCurrentSceneSettings.database->FixTables.at(name);
        return FixTableProxy{
            .DataPtr     = const_cast<uint8*>(fixtable.data().data()),
            .SizeInBytes = fixtable.data().size() * sizeof(uint8)
        };
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
                std::move(UnifiedArray<Node2>::CreateExternalOrAllocateDevice(mDeviceID, (Node2*)bvh.Nodes.data(), node_count, !isGPU())),
                std::move(UnifiedArray<EntityLeaf1>::CreateExternalOrAllocateDevice(mDeviceID, (EntityLeaf1*)bvh.Leaves.data(), leaf_count, !isGPU()))
            };
        } break;
        case BVHType::BVH4: {
            const size_t node_count = bvh.Nodes.size() / sizeof(Node4);
            device.bvh_ents[str]    = IG::Bvh4Ent{
                std::move(UnifiedArray<Node4>::CreateExternalOrAllocateDevice(mDeviceID, (Node4*)bvh.Nodes.data(), node_count, !isGPU())),
                std::move(UnifiedArray<EntityLeaf1>::CreateExternalOrAllocateDevice(mDeviceID, (EntityLeaf1*)bvh.Leaves.data(), leaf_count, !isGPU()))
            };
        } break;
        case BVHType::BVH8: {
            const size_t node_count = bvh.Nodes.size() / sizeof(Node8);
            device.bvh_ents[str]    = IG::Bvh8Ent{
                std::move(UnifiedArray<Node8>::CreateExternalOrAllocateDevice(mDeviceID, (Node8*)bvh.Nodes.data(), node_count, !isGPU())),
                std::move(UnifiedArray<EntityLeaf1>::CreateExternalOrAllocateDevice(mDeviceID, (EntityLeaf1*)bvh.Leaves.data(), leaf_count, !isGPU()))
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
        *nodes    = const_cast<Node2*>(bvh.Nodes.DevicePtr);
        *objs     = const_cast<EntityLeaf1*>(bvh.Objs.DevicePtr);
    } break;
    case BVHType::BVH4: {
        auto& bvh = std::get<IG::Bvh4Ent>(it->second);
        *nodes    = const_cast<Node4*>(bvh.Nodes.DevicePtr);
        *objs     = const_cast<EntityLeaf1*>(bvh.Objs.DevicePtr);
    } break;
    case BVHType::BVH8: {
        auto& bvh = std::get<IG::Bvh8Ent>(it->second);
        *nodes    = const_cast<Node8*>(bvh.Nodes.DevicePtr);
        *objs     = const_cast<EntityLeaf1*>(bvh.Objs.DevicePtr);
    } break;
    }
}

IDeviceInterface::DeviceImageProxy<float> DeviceInterface::loadImageFromFile(const std::string& filename, int32_t expected_channels)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    auto& images = mDeviceData.images;
    if (const auto it = images.find(filename); it != images.end())
        return mapToProxy(it->second);

    _SECTION(SectionType::ImageLoading);

    IG_LOG(L_DEBUG) << "Loading image '" << filename << "' (C=" << expected_channels << ")" << std::endl;
    UnifiedArray<float> arr;
    size_t width, height;
    try {
        Image image = Image::load(filename);
        if (image.channels != (size_t)expected_channels)
            image = image.castTo((size_t)expected_channels);

        arr    = UnifiedArray<float>::AllocateDevice(mDeviceID, image.width * image.height * image.channels);
        width  = image.width;
        height = image.height;
        arr.copyFromExternalHostToDevice(image.pixels.get());

        auto& res = getCurrentShader().images[filename]; // Get or construct resource info for given resource
        res.counter++;
        res.memory_usage = arr.SizeInBytes;
    } catch (const ImageLoadException& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;

        if (MissingImage.channels != (size_t)expected_channels) {
            Image image = MissingImage.castTo((size_t)expected_channels);
            arr         = UnifiedArray<float>::AllocateDevice(mDeviceID, image.width * image.height * image.channels);
            width       = image.width;
            height      = image.height;
            arr.copyFromExternalHostToDevice(image.pixels.get());
        } else {
            arr    = UnifiedArray<float>::AllocateDevice(mDeviceID, MissingImage.width * MissingImage.height * MissingImage.channels);
            width  = MissingImage.width;
            height = MissingImage.height;
            arr.copyFromExternalHostToDevice(MissingImage.pixels.get());
        }
    }

    return mapToProxy(images.emplace(filename, DeviceImage{
                                                   .Data   = std::move(arr),
                                                   .Width  = width,
                                                   .Height = height })
                          .first->second);
}

IDeviceInterface::DeviceImageProxy<uint8_t> DeviceInterface::loadPackedImageFromFile(const std::string& filename, int32_t expected_channels, bool linear)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    auto& images = mDeviceData.packed_images;
    if (const auto it = images.find(filename); it != images.end())
        return mapToProxy(it->second);

    _SECTION(SectionType::PackedImageLoading);

    IG_LOG(L_DEBUG) << "Loading (packed) image '" << filename << "' (C=" << expected_channels << ")" << std::endl;
    std::vector<uint8> hostData;
    size_t width, height, channels;
    try {
        Image::loadAsPacked(filename, hostData, width, height, channels, linear);

        if (expected_channels != (int32_t)channels) {
            IG_LOG(L_ERROR) << "Packed image '" << filename << "' is has unexpected channel count" << std::endl;
            MissingImage.copyToPackedFormat(hostData);
            width    = MissingImage.width;
            height   = MissingImage.height;
            channels = MissingImage.channels;
        } else {
            auto& res = getCurrentShader().packed_images[filename]; // Get or construct resource info for given resource
            res.counter++;
            res.memory_usage = hostData.size();
        }
    } catch (const ImageLoadException& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        MissingImage.copyToPackedFormat(hostData);
        width    = MissingImage.width;
        height   = MissingImage.height;
        channels = MissingImage.channels;
    }

    auto arr = UnifiedArray<uint8>::AllocateDevice(mDeviceID, hostData.size());
    arr.copyFromExternalHostToDevice(hostData.data());

    return mapToProxy(images.emplace(filename, DevicePackedImage{
                                                   .Data   = std::move(arr),
                                                   .Width  = width,
                                                   .Height = height })
                          .first->second);
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

    auto& buffers = mDeviceData.buffers;
    if (const auto it = buffers.find(filename); it != buffers.end())
        return mapToProxy(it->second);

    IG_LOG(L_DEBUG) << "Loading buffer '" << filename << "'" << std::endl;
    const auto vec = readBufferFile(filename);

    if ((vec.size() % sizeof(int32_t)) != 0)
        IG_LOG(L_WARNING) << "Buffer '" << filename << "' is not properly sized!" << std::endl;

    auto arr = UnifiedArray<uint8>::AllocateDevice(mDeviceID, vec.size());
    arr.copyFromExternalHostToDevice(vec.data());

    return mapToProxy(buffers.emplace(filename, std::move(arr)).first->second);
}

IDeviceInterface::DeviceBufferProxy<uint8_t> DeviceInterface::loadBufferByName(const std::string& name)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    if (const auto it = mDeviceData.buffers.find(name); it != mDeviceData.buffers.end())
        return mapToProxy(it->second);

    IG_LOG(L_ERROR) << "No buffer '" << name << "'" << std::endl;

    return DeviceBufferProxy<uint8_t>::Invalid();
}

IDeviceInterface::DeviceBufferProxy<uint8_t> DeviceInterface::requestBuffer(const std::string& name, int32_t size, int32_t flags)
{
    IG_UNUSED(flags); // We do not make use of it yet

    std::lock_guard<std::mutex> _guard(mThreadMutex);

    IG_ASSERT(size > 0, "Expected buffer size to be larger then zero");

    // Make sure the buffer is properly sized
    size = (int32_t)roundUp(size, 32);

    auto& buffers = mDeviceData.buffers;
    if (const auto it = buffers.find(name); it != buffers.end() && it->second.Data.SizeInBytes >= size * sizeof(uint8))
        return mapToProxy(it->second);

    IG_LOG(L_DEBUG) << "Requested buffer '" << name << "' with " << FormatMemory(size) << std::endl;

    auto arr = UnifiedArray<uint8>::AllocateDevice(mDeviceID, size);
    return mapToProxy(buffers.emplace(name, std::move(arr)).first->second);
}

void DeviceInterface::saveBufferToFile(const std::string& name, const std::string& filename)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);
    if (const auto it = mDeviceData.buffers.find(name); it != mDeviceData.buffers.end()) {
        const size_t size = (size_t)it->second.Data.SizeInBytes;

        IG_LOG(L_DEBUG) << "Dumping buffer '" << name << "' to '" << filename << "' with " << FormatMemory(size) << std::endl;

        // Copy data to host
        std::vector<uint8_t> host_data(size);
        it->second.Data.copyFromDeviceToExternalHost(host_data.data());

        // Dump data as binary glob
        std::ofstream out(filename);
        out.write(reinterpret_cast<const char*>(host_data.data()), host_data.size());
        out.close();
    } else {
        IG_LOG(L_WARNING) << "Buffer '" << name << "' can not be dumped as it does not exists" << std::endl;
    }
}

bool DeviceInterface::copyBufferToHost(const std::string& buffer_name, void* dst)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    if (const auto it = mDeviceData.buffers.find(buffer_name); it != mDeviceData.buffers.end()) {
        it->second.Data.copyFromDeviceToExternalHost((uint8*)dst);
        return true;
    }
    return false;
}

bool DeviceInterface::copyBufferFromHost(const std::string& buffer_name, const void* src)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    if (const auto it = mDeviceData.buffers.find(buffer_name); it != mDeviceData.buffers.end()) {
        it->second.Data.copyFromExternalHostToDevice((const uint8*)src);
        return true;
    }
    return false;
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

    std::lock_guard<std::mutex> _guard(mThreadMutex);
    if (const auto it = mDeviceData.buffers.find("__dbg_output"); it != mDeviceData.buffers.end()) {
        DeviceBuffer& buffer = it->second;

        std::vector<uint8> host_data(isGPU() ? buffer.Data.SizeInBytes : 0);
        if (isGPU())
            buffer.Data.copyFromDeviceToExternalHost(host_data.data());

        // Parse data
        int32_t* ptr  = reinterpret_cast<int32_t*>(!isGPU() ? buffer.Data.DevicePtr : host_data.data());
        int32_t occup = std::min(ptr[0], static_cast<int32_t>(buffer.Data.SizeInBytes / sizeof(int32_t)));

        if (occup <= 0)
            return;

        handleDebug(ptr, occup);

        if (isGPU())
            buffer.Data.copyFromExternalHostToDevice(host_data.data());
    }
}

IDeviceInterface::DeviceImageProxy<float> DeviceInterface::loadAOVImageForDevice(const std::string& aov_name)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    const std::string& actual_name = handleAOVName(aov_name);

    if (const auto it = mDeviceData.aovs.find(actual_name); it != mDeviceData.aovs.end()) {
        it->second.Data.syncForDevice();
        return mapToProxy(it->second);
    } else {
        const size_t expectedSize = framebufferArea() * 3;

        auto& aov = mDeviceData.aovs.emplace(actual_name,
                                             DeviceImage{
                                                 .Data   = UnifiedArray<float>::AllocateUnified(mDeviceID, expectedSize),
                                                 .Width  = mFramebufferWidth,
                                                 .Height = mFramebufferHeight })
                        .first->second;
        aov.Data.fillWithZero();
        return mapToProxy(aov);
    }
}

IDeviceInterface::DeviceImageProxy<float> DeviceInterface::loadAOVImageForHost(const std::string& aov_name)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    const std::string& actual_name = handleAOVName(aov_name);

    if (const auto it = mDeviceData.aovs.find(actual_name); it != mDeviceData.aovs.end()) {
        it->second.Data.syncForHost();
        return DeviceImageProxy<float>{
            .DataPtr = it->second.Data.HostPtr,
            .Width   = it->second.Width,
            .Height  = it->second.Height
        };
    } else {
        IG_LOG(L_ERROR) << "Unknown aov '" << actual_name << "' access for host" << std::endl;
        return DeviceImageProxy<float>::Invalid();
    }
}

/// Clear specific aov
void DeviceInterface::clearAOV(const std::string& aov_name)
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    const std::string& actual_name = handleAOVName(aov_name);
    if (const auto it = mDeviceData.aovs.find(actual_name); it != mDeviceData.aovs.end())
        it->second.Data.fillWithZero();
}

/// Clear all aovs and the framebuffer
void DeviceInterface::clearAllAOVs()
{
    std::lock_guard<std::mutex> _guard(mThreadMutex);

    for (auto& p : mDeviceData.aovs)
        p.second.Data.fillWithZero();
}

// -------------------------------------------------------- Shader
void DeviceInterface::runDeviceShader(const TechniqueVariantShaderSet& shaderSet, const Device::RenderSettings& settings)
{
    DeviceGuard _guard(this);
    updateShaderSet(shaderSet);
    updateSettings(settings);

    ensureFramebuffer();

    mCurrentRenderSettings = settings;

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

    markFramebufferDirty();
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
    setCurrentShader(last - first, ShaderKey(mCurrentShaderSet.ID, shaderType, material_id >= 0 ? (uint32)material_id : 0), output);
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
    // A pass shader might not access the framebuffer!

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
    getCurrentThreadData()->stats.beginSection((IG::SectionType)id);
}

void DeviceInterface::endStatsSection(int id)
{
    getCurrentThreadData()->stats.endSection((IG::SectionType)id);
}

void DeviceInterface::addStatsValue(int id, int value)
{
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