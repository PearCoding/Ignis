// #define IG_DEBUG_LOG_TRACE

#include "Device.h"
#include "Image.h"
#include "Logger.h"
#include "RuntimeStructs.h"
#include "ShaderKey.h"
#include "Statistics.h"
#include "table/SceneDatabase.h"

#include "generated_interface.h"

#include "ShallowArray.h"

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
    size_t ref_count = 0;
    DeviceStream cpu_primary;
    DeviceStream cpu_secondary;
    TemporaryStorageHostProxy temporary_storage_host;
    Statistics stats;
    const ParameterSet* current_local_registry = nullptr;
    ShaderKey current_shader_key               = ShaderKey(0, ShaderType::Device, 0);
    std::unordered_map<ShaderKey, ShaderStats, ShaderKeyHash> shader_stats;
};
thread_local CPUData* tlThreadData = nullptr;

#ifdef IG_HAS_DENOISER
void ignis_denoise(const float*, const float*, const float*, const float*, float*, size_t, size_t, size_t);
#endif

constexpr size_t GPUStreamBufferCount = 2;
class Interface {
    IG_CLASS_NON_COPYABLE(Interface);
    IG_CLASS_NON_MOVEABLE(Interface);

public:
    class DeviceData {
        IG_CLASS_NON_COPYABLE(DeviceData);
        IG_CLASS_NON_MOVEABLE(DeviceData);

    public:
        std::unordered_map<std::string, BvhVariant> bvh_ents;
        anydsl::Array<int32_t> tmp_buffer;
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

        const ParameterSet* current_local_registry = nullptr;
        ShaderKey current_shader_key;

        inline DeviceData()
            : current_primary()
            , current_secondary()
            , current_shader_key(0, ShaderType::Device, 0)
        {
            for (size_t i = 0; i < primary.size(); ++i)
                current_primary[i] = &primary[i];
            for (size_t i = 0; i < secondary.size(); ++i)
                current_secondary[i] = &secondary[i];
        }

        ~DeviceData() = default;
    };
    std::unordered_map<int32_t, DeviceData> devices;

    std::mutex thread_mutex;
    std::vector<std::unique_ptr<CPUData>> thread_data;

    tbb::concurrent_queue<CPUData*> available_thread_data;
    std::unordered_map<ShaderKey, ShaderInfo, ShaderKeyHash> shader_infos;

    std::unordered_map<std::string, AOV> aovs;
    AOV host_pixels;

    const SceneDatabase* database;
    const size_t entity_count;
    size_t film_width;
    size_t film_height;

    const Device::SetupSettings setup;
    Device::RenderSettings current_settings;
    const ParameterSet* current_parameters = nullptr;
    TechniqueVariantShaderSet shader_set;

    Statistics main_stats;

    Settings driver_settings;

    const bool is_gpu;

    inline explicit Interface(const Device::SetupSettings& setup)
        : database(setup.database)
        , entity_count(database->FixTables.count("entities") > 0 ? database->FixTables.at("entities").entryCount() : 0)
        , film_width(setup.framebuffer_width)
        , film_height(setup.framebuffer_height)
        , setup(setup)
        , main_stats()
        , driver_settings()
        , is_gpu(setup.target.isGPU())
    {
        driver_settings.device       = (int)setup.target.device();
        driver_settings.thread_count = (int)setup.target.threadCount();
        updateSettings(Device::RenderSettings{}); // Initialize with default values

        ensureSetup();
        setupThreadData();
    }

    inline ~Interface() = default;

    inline void ensureSetup()
    {
        if (host_pixels.Data.data())
            return;

        setupFramebuffer();
    }

    inline int getDevID(size_t device) const
    {
        switch (setup.target.gpuVendor()) {
        default:
            return 0;
        case GPUVendor::Nvidia:
            return ANYDSL_DEVICE(ANYDSL_CUDA, (int)device);
        case GPUVendor::AMD:
            return ANYDSL_DEVICE(ANYDSL_HSA, (int)device);
        }
    }
    inline int getDevID() const { return getDevID(setup.target.device()); }

    inline size_t getPrimaryPayloadBlockSize() const { return current_settings.info.PrimaryPayloadCount; }
    inline size_t getSecondaryPayloadBlockSize() const { return current_settings.info.SecondaryPayloadCount; }

    inline const std::string& lookupResource(int32_t id) const
    {
        return setup.resource_map->at(id);
    }

    inline void setupFramebuffer()
    {
        if (setup.aov_map) {
            for (const auto& name : *setup.aov_map)
                aovs.emplace(name, AOV{});
        }

        host_pixels.Data = anydsl::Array<float>(film_width * film_height * 3);

#ifdef IG_HAS_DENOISER
        if (aovs.count("Denoised") != 0)
            aovs["Denoised"].HostOnly = true; // Always mapped, ignore data from device
#endif

        for (auto& p : aovs)
            p.second.Data = anydsl::Array<float>(film_width * film_height * 3);

        resetFramebufferAccess();
    }

    inline void resizeFramebuffer(size_t width, size_t height)
    {
        IG_ASSERT(width > 0 && height > 0, "Expected given width & height to be greater than 0");

        if (film_width == width && film_height == height)
            return;

        film_width  = width;
        film_height = height;
        setupFramebuffer();
    }

    inline void resetFramebufferAccess()
    {
        host_pixels.Mapped = false;

        for (auto& p : aovs)
            p.second.Mapped = false;
    }

    template <typename T>
    inline anydsl::Array<T>& resizeArray(int32_t dev, anydsl::Array<T>& array, size_t size, size_t multiplier)
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

    inline void setupThreadData()
    {
        tlThreadData = nullptr;
        thread_data.clear();
        if (is_gpu) {
            tlThreadData            = thread_data.emplace_back(std::make_unique<CPUData>()).get(); // Just one single data available...
            tlThreadData->ref_count = 1;
        } else {
            const size_t req_threads = setup.target.threadCount() == 0 ? std::thread::hardware_concurrency() : setup.target.threadCount();
            const size_t max_threads = req_threads + 1 /* Host */;

            available_thread_data.clear();
            for (size_t t = 0; t < max_threads; ++t) {
                CPUData* ptr = thread_data.emplace_back(std::make_unique<CPUData>()).get();
                available_thread_data.push(ptr);
            }
        }
    }

    inline void updateSettings(const Device::RenderSettings& settings)
    {
        driver_settings.spi    = (int)settings.spi;
        driver_settings.frame  = (int)settings.frame;
        driver_settings.iter   = (int)settings.iteration;
        driver_settings.width  = (int)settings.work_width;
        driver_settings.height = (int)settings.work_height;
    }

    inline void setupShaderSet(const TechniqueVariantShaderSet& shaderSet)
    {
        shader_set = shaderSet;

        // Prepare cache data
        shader_infos[ShaderKey(shader_set.ID, ShaderType::Device, 0)] = {};
        if (shader_set.TonemapShader.Exec) {
            shader_infos[ShaderKey(shader_set.ID, ShaderType::Tonemap, 0)]   = {};
            shader_infos[ShaderKey(shader_set.ID, ShaderType::ImageInfo, 0)] = {};
        }

        shader_infos[ShaderKey(shader_set.ID, ShaderType::PrimaryTraversal, 0)]   = {};
        shader_infos[ShaderKey(shader_set.ID, ShaderType::SecondaryTraversal, 0)] = {};
        shader_infos[ShaderKey(shader_set.ID, ShaderType::RayGeneration, 0)]      = {};
        shader_infos[ShaderKey(shader_set.ID, ShaderType::Miss, 0)]               = {};
        for (size_t i = 0; i < shader_set.HitShaders.size(); ++i)
            shader_infos[ShaderKey(shader_set.ID, ShaderType::Hit, (uint32)i)] = {};
        for (size_t i = 0; i < shader_set.AdvancedShadowHitShaders.size(); ++i)
            shader_infos[ShaderKey(shader_set.ID, ShaderType::AdvancedShadowHit, (uint32)i)] = {};
        for (size_t i = 0; i < shader_set.AdvancedShadowMissShaders.size(); ++i)
            shader_infos[ShaderKey(shader_set.ID, ShaderType::AdvancedShadowMiss, (uint32)i)] = {};
        for (size_t i = 0; i < shader_set.CallbackShaders.size(); ++i)
            shader_infos[ShaderKey(shader_set.ID, ShaderType::Callback, (uint32)i)] = {};
    }

    inline void registerThread()
    {
        if (is_gpu)
            return;

        if (tlThreadData == nullptr) {
            CPUData* ptr = nullptr;
            while (!available_thread_data.try_pop(ptr))
                std::this_thread::yield();

            if (ptr == nullptr)
                IG_LOG(L_FATAL) << "Registering thread 0x" << std::hex << std::this_thread::get_id() << " failed!" << std::endl;
            else
                tlThreadData = ptr;

            tlThreadData->ref_count = 0;
        }

        tlThreadData->ref_count += 1;
    }

    inline void unregisterThread()
    {
        if (is_gpu)
            return;

        IG_ASSERT(tlThreadData != nullptr, "Expected registerThread together with a unregisterThread");

        if (tlThreadData->ref_count <= 1) {
            tlThreadData->ref_count = 0;
            available_thread_data.push(tlThreadData);
            tlThreadData = nullptr;
        } else {
            tlThreadData->ref_count -= 1;
        }
    }

    inline CPUData* getThreadData()
    {
        IG_ASSERT(tlThreadData != nullptr, "Thread not registered");
        return tlThreadData;
    }

    inline void setCurrentShader(int32_t dev, int workload, const ShaderKey& key, const ShaderOutput<void*>& shader)
    {
        if (is_gpu) {
            devices[dev].current_local_registry = &shader.LocalRegistry;
            devices[dev].current_shader_key     = key;
            auto data                           = getThreadData();
            data->shader_stats[key].call_count++;
            data->shader_stats[key].workload_count += (size_t)workload;
        } else {
            auto data                    = getThreadData();
            data->current_local_registry = &shader.LocalRegistry;
            data->current_shader_key     = key;
            data->shader_stats[key].call_count++;
            data->shader_stats[key].workload_count += (size_t)workload;
        }
    }

    inline const ParameterSet* getCurrentLocalRegistry(int32_t dev)
    {
        if (is_gpu)
            return devices[dev].current_local_registry;
        else
            return getThreadData()->current_local_registry;
    }

    inline ShaderInfo& getCurrentShaderInfo(int32_t dev)
    {
        if (is_gpu)
            return shader_infos.at(devices[dev].current_shader_key);
        else
            return shader_infos.at(getThreadData()->current_shader_key);
    }

    inline DeviceStream& getPrimaryStream(int32_t dev, size_t buffer, size_t size)
    {
        const size_t elements = roundUp(MinPrimaryStreamSize + getPrimaryPayloadBlockSize(), 4);
        auto& stream          = is_gpu ? *devices[dev].current_primary.at(buffer) : getThreadData()->cpu_primary;
        resizeArray(dev, stream.Data, size, elements);
        stream.BlockSize = size;

        return stream;
    }

    inline DeviceStream& getPrimaryStream(int32_t dev, size_t buffer)
    {
        if (is_gpu) {
            IG_ASSERT(devices[dev].current_primary.at(buffer)->Data.size() > 0, "Expected gpu primary stream to be initialized");
            return *devices[dev].current_primary.at(buffer);
        } else {
            IG_ASSERT(getThreadData()->cpu_primary.Data.size() > 0, "Expected cpu primary stream to be initialized");
            return getThreadData()->cpu_primary;
        }
    }

    inline DeviceStream& getSecondaryStream(int32_t dev, size_t buffer, size_t size)
    {
        const size_t elements = roundUp(MinSecondaryStreamSize + getSecondaryPayloadBlockSize(), 4);
        auto& stream          = is_gpu ? *devices[dev].current_secondary.at(buffer) : getThreadData()->cpu_secondary;
        resizeArray(dev, stream.Data, size, elements);
        stream.BlockSize = size;

        return stream;
    }

    inline DeviceStream& getSecondaryStream(int32_t dev, size_t buffer)
    {
        if (is_gpu) {
            IG_ASSERT(devices[dev].current_secondary.at(buffer)->Data.size() > 0, "Expected gpu secondary stream to be initialized");
            return *devices[dev].current_secondary.at(buffer);
        } else {
            IG_ASSERT(getThreadData()->cpu_secondary.Data.size() > 0, "Expected cpu secondary stream to be initialized");
            return getThreadData()->cpu_secondary;
        }
    }

    inline anydsl::Array<int32_t>& getGPUTemporaryBuffer(int32_t dev)
    {
        return resizeArray(dev, devices[dev].tmp_buffer, getTemporaryBufferSize(), 1);
    }

    inline void swapGPUPrimaryStreams(int32_t dev)
    {
        auto& device = devices[dev];
        std::swap(device.current_primary[0], device.current_primary[1]);
    }

    inline void swapGPUSecondaryStreams(int32_t dev)
    {
        auto& device = devices[dev];
        std::swap(device.current_secondary[0], device.current_secondary[1]);
    }

    inline size_t getTemporaryBufferSize() const
    {
        // Upper bound extracted from "mapping_*.art"
        return roundUp(std::max<size_t>(32, std::max(entity_count + 1, database->MaterialCount * 2)), 4);
    }

    inline const auto& getTemporaryStorageHost(int32_t dev)
    {
        const size_t size = getTemporaryBufferSize();
        if (dev == 0) {
            auto thread = getThreadData();

            return thread->temporary_storage_host = TemporaryStorageHostProxy{
                std::move(resizeArray(0 /*Host*/, thread->temporary_storage_host.ray_begins, size, 1)),
                std::move(resizeArray(0 /*Host*/, thread->temporary_storage_host.ray_ends, size, 1))
            };
        } else {
            auto& device = devices[dev];

            return device.temporary_storage_host = TemporaryStorageHostProxy{
                std::move(resizeArray(0 /*Host*/, device.temporary_storage_host.ray_begins, size, 1)),
                std::move(resizeArray(0 /*Host*/, device.temporary_storage_host.ray_ends, size, 1))
            };
        }
    }

    template <typename Bvh, typename Node>
    inline const Bvh& loadEntityBVH(int32_t dev, const char* prim_type)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& device    = devices[dev];
        std::string str = prim_type;
        auto it         = device.bvh_ents.find(str);
        if (it == device.bvh_ents.end())
            return std::get<Bvh>(device.bvh_ents.emplace(str, std::move(loadSceneBVH<Node>(dev, prim_type))).first->second);
        else
            return std::get<Bvh>(it->second);
    }

    inline const anydsl::Array<StreamRay>& loadRayList(int32_t dev)
    {
        size_t count = current_settings.work_width;
        auto& device = devices[dev];
        if (device.ray_list.size() == (int64_t)count)
            return device.ray_list;

        IG_ASSERT(current_settings.rays != nullptr, "Expected list of rays to be available");

        std::vector<StreamRay> rays;
        rays.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            const auto dRay = current_settings.rays[i];

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

        return device.ray_list = copyToDevice(dev, rays);
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
            return anydsl::Array<T>();
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
        return DeviceImage{ copyToDevice(dev, image.pixels.get(), image.width * image.height * 4), image.width, image.height };
    }

    inline DevicePackedImage copyToDevicePacked(int32_t dev, const Image& image)
    {
        std::vector<uint8_t> packed;
        image.copyToPackedFormat(packed);
        return DevicePackedImage{ copyToDevice(dev, packed), image.width, image.height };
    }

    template <typename Node>
    inline BvhProxy<Node, EntityLeaf1> loadSceneBVH(int32_t dev, const char* prim_type)
    {
        IG_LOG(L_DEBUG) << "Loading scene bvh " << prim_type << std::endl;
        const auto& bvh         = database->SceneBVHs.at(prim_type);
        const size_t node_count = bvh.Nodes.size() / sizeof(Node);
        const size_t leaf_count = bvh.Leaves.size() / sizeof(EntityLeaf1);
        return BvhProxy<Node, EntityLeaf1>{
            std::move(ShallowArray<Node>(dev, reinterpret_cast<const Node*>(bvh.Nodes.data()), node_count)),
            std::move(ShallowArray<EntityLeaf1>(dev, reinterpret_cast<const EntityLeaf1*>(bvh.Leaves.data()), leaf_count))
        };
    }

    inline DynTableProxy loadDyntable(int32_t dev, const DynTable& tbl)
    {
        static_assert(sizeof(::LookupEntry) == sizeof(LookupEntry), "Expected generated Lookup Entry and internal Lookup Entry to be of same size!");

        DynTableProxy proxy;
        proxy.EntryCount    = tbl.entryCount();
        proxy.LookupEntries = ShallowArray<::LookupEntry>(dev, (::LookupEntry*)tbl.lookups().data(), tbl.lookups().size());
        proxy.Data          = ShallowArray<uint8_t>(dev, tbl.data().data(), tbl.data().size());
        return proxy;
    }

    inline DeviceBuffer loadFixtable(int32_t dev, const FixTable& tbl)
    {
        return DeviceBuffer{ copyToDevice(dev, tbl.data()), 1 };
    }

    inline SceneInfo loadSceneInfo(int32_t dev)
    {
        IG_UNUSED(dev);

        return SceneInfo{ (int)entity_count,
                          (int)database->MaterialCount };
    }

    inline const DynTableProxy& loadDyntable(int32_t dev, const char* name)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& tables = devices[dev].dyntables;
        auto it      = tables.find(name);
        if (it != tables.end())
            return it->second;

        IG_LOG(L_DEBUG) << "Loading dyntable '" << name << "'" << std::endl;

        return tables[name] = loadDyntable(dev, database->DynTables.at(name));
    }

    inline const DeviceBuffer& loadFixtable(int32_t dev, const char* name)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& tables = devices[dev].fixtables;
        auto it      = tables.find(name);
        if (it != tables.end())
            return it->second;

        IG_LOG(L_DEBUG) << "Loading fixtable '" << name << "'" << std::endl;

        return tables[name] = loadFixtable(dev, database->FixTables.at(name));
    }

    inline const DeviceImage& loadImage(int32_t dev, const std::string& filename, int32_t expected_channels)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& images = devices[dev].images;
        auto it      = images.find(filename);
        if (it != images.end())
            return it->second;

        _SECTION(SectionType::ImageLoading);

        IG_LOG(L_DEBUG) << "Loading image '" << filename << "' (C=" << expected_channels << ")" << std::endl;
        try {
            const auto img = Image::load(filename);
            if (expected_channels != (int32_t)img.channels) {
                IG_LOG(L_ERROR) << "Image '" << filename << "' is has unexpected channel count" << std::endl;
                return images[filename] = copyToDevice(dev, Image());
            }

            auto& res = getCurrentShaderInfo(dev).images[filename];
            res.counter++;
            res.memory_usage        = img.width * img.height * img.channels * sizeof(float);
            return images[filename] = copyToDevice(dev, img);
        } catch (const ImageLoadException& e) {
            IG_LOG(L_ERROR) << e.what() << std::endl;
            return images[filename] = copyToDevice(dev, Image());
        }
    }

    inline const DevicePackedImage& loadPackedImage(int32_t dev, const std::string& filename, int32_t expected_channels, bool linear)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& images = devices[dev].packed_images;
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
                return images[filename] = DevicePackedImage{ copyToDevice(dev, std::vector<uint8_t>{}), 0, 0 };
            }

            auto& res = getCurrentShaderInfo(dev).packed_images[filename];
            res.counter++;
            res.memory_usage        = packed.size();
            return images[filename] = DevicePackedImage{ copyToDevice(dev, packed), width, height };
        } catch (const ImageLoadException& e) {
            IG_LOG(L_ERROR) << e.what() << std::endl;
            return images[filename] = DevicePackedImage{ copyToDevice(dev, std::vector<uint8_t>{}), 0, 0 };
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

    inline const DeviceBuffer& loadBuffer(int32_t dev, const std::string& filename)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& buffers = devices[dev].buffers;
        auto it       = buffers.find(filename);
        if (it != buffers.end())
            return it->second;

        _SECTION(SectionType::BufferLoading);

        IG_LOG(L_DEBUG) << "Loading buffer '" << filename << "'" << std::endl;
        const auto vec = readBufferFile(filename);

        if ((vec.size() % sizeof(int32_t)) != 0)
            IG_LOG(L_WARNING) << "Buffer '" << filename << "' is not properly sized!" << std::endl;

        return buffers[filename] = DeviceBuffer{ copyToDevice(dev, vec), 1 };
    }

    inline DeviceBuffer& requestBuffer(int32_t dev, const std::string& name, int32_t size, int32_t flags)
    {
        IG_UNUSED(flags); // We do not make use of it yet

        std::lock_guard<std::mutex> _guard(thread_mutex);

        IG_ASSERT(size > 0, "Expected buffer size to be larger then zero");

        // Make sure the buffer is properly sized
        size = (int32_t)roundUp(size, 32);

        auto& buffers = devices[dev].buffers;
        auto it       = buffers.find(name);
        if (it != buffers.end() && it->second.Data.size() >= (int64_t)size) {
            return it->second;
        }

        _SECTION(SectionType::BufferRequests);

        IG_LOG(L_DEBUG) << "Requested buffer '" << name << "' with " << FormatMemory(size) << std::endl;

        void* ptr = anydsl_alloc(dev, size);
        if (ptr == nullptr) {
            IG_LOG(L_FATAL) << "Out of memory" << std::endl;
            std::abort();
        }

        return buffers[name] = DeviceBuffer{ anydsl::Array<uint8_t>(dev, reinterpret_cast<uint8_t*>(ptr), size), 1 };
    }

    inline void dumpBuffer(int32_t dev, const std::string& name, const std::string& filename)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& buffers = devices[dev].buffers;
        auto it       = buffers.find(name);
        if (it != buffers.end()) {
            const size_t size = (size_t)it->second.Data.size();

            // Copy data to host
            std::vector<uint8_t> host_data(size);
            anydsl_copy(dev, it->second.Data.data(), 0, 0 /* Host */, host_data.data(), 0, host_data.size());

            // Dump data as binary glob
            std::ofstream out(filename);
            out.write(reinterpret_cast<const char*>(host_data.data()), host_data.size());
            out.close();
        } else {
            IG_LOG(L_WARNING) << "Buffer '" << name << "' can not be dumped as it does not exists" << std::endl;
        }
    }

    inline void dumpDebugBuffer(int32_t dev, DeviceBuffer& buffer)
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

        if (dev != 0) {
            // Copy data to host
            std::vector<uint8_t> host_data((size_t)buffer.Data.size());
            anydsl_copy(dev, buffer.Data.data(), 0, 0 /* Host */, host_data.data(), 0, host_data.size());

            // Parse data
            int32_t* ptr  = reinterpret_cast<int32_t*>(host_data.data());
            int32_t occup = std::min(ptr[0], static_cast<int32_t>(host_data.size() / sizeof(int32_t)));

            if (occup <= 0)
                return;

            handleDebug(ptr, occup);

            // Copy back to device
            anydsl_copy(0 /* Host */, host_data.data(), 0, dev, buffer.Data.data(), 0, sizeof(int32_t));
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
        for (auto& dev : devices) {
            if (dev.second.buffers.count("__dbg_output"))
                dumpDebugBuffer(dev.first, dev.second.buffers["__dbg_output"]);
        }
    }

    inline void runDeviceShader()
    {
#ifdef IG_DEBUG_LOG_TRACE
        IG_LOG(L_DEBUG) << "TRACE> Device Shader" << std::endl;
#endif

        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::Device, 1, {});

        using Callback = decltype(ig_callback_shader);
        auto callback  = reinterpret_cast<Callback*>(shader_set.DeviceShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected device shader to be valid");
        setCurrentShader(0, 1, ShaderKey(shader_set.ID, ShaderType::Device, 0), shader_set.DeviceShader);
        callback(&driver_settings);

        checkDebugOutput();

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(ShaderType::Device, {});
    }

    inline void runTonemapShader(float* in_pixels, uint32_t* device_out_pixels, ::TonemapSettings& settings)
    {
#ifdef IG_DEBUG_LOG_TRACE
        IG_LOG(L_DEBUG) << "TRACE> Tonemap Shader" << std::endl;
#endif

        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::Tonemap, 1, {});

        using Callback = decltype(ig_tonemap_shader);
        auto callback  = reinterpret_cast<Callback*>(shader_set.TonemapShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected tonemap shader to be valid");
        setCurrentShader(0, 1, ShaderKey(shader_set.ID, ShaderType::Tonemap, 0), shader_set.TonemapShader);
        callback(&driver_settings, in_pixels, device_out_pixels, (int)film_width, (int)film_height, &settings);

        checkDebugOutput();

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(ShaderType::Tonemap, {});
    }

    inline ::ImageInfoOutput runImageinfoShader(float* in_pixels, ::ImageInfoSettings& settings)
    {
#ifdef IG_DEBUG_LOG_TRACE
        IG_LOG(L_DEBUG) << "TRACE> Imageinfo Shader" << std::endl;
#endif

        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::ImageInfo, 1, {});

        using Callback = decltype(ig_imageinfo_shader);
        auto callback  = reinterpret_cast<Callback*>(shader_set.ImageinfoShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected imageinfo shader to be valid");
        setCurrentShader(0, 1, ShaderKey(shader_set.ID, ShaderType::ImageInfo, 0), shader_set.ImageinfoShader);

        ::ImageInfoOutput output;
        callback(&driver_settings, in_pixels, (int)film_width, (int)film_height, &settings, &output);

        checkDebugOutput();

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(ShaderType::ImageInfo, {});

        return output;
    }

    inline void runPrimaryTraversalShader(int32_t dev, int size)
    {
#ifdef IG_DEBUG_LOG_TRACE
        IG_LOG(L_DEBUG) << "TRACE> Primary Traversal Shader [S=" << size << "]" << std::endl;
#endif

        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::PrimaryTraversal, size, {});

        using Callback = decltype(ig_traversal_shader);
        auto callback  = reinterpret_cast<Callback*>(shader_set.PrimaryTraversalShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected primary traversal shader to be valid");
        setCurrentShader(dev, size, ShaderKey(shader_set.ID, ShaderType::PrimaryTraversal, 0), shader_set.PrimaryTraversalShader);
        callback(&driver_settings, size);

        checkDebugOutput();

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(ShaderType::PrimaryTraversal, {});
    }

    inline void runSecondaryTraversalShader(int32_t dev, int size)
    {
#ifdef IG_DEBUG_LOG_TRACE
        IG_LOG(L_DEBUG) << "TRACE> Secondary Traversal Shader [S=" << size << "]" << std::endl;
#endif

        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::SecondaryTraversal, size, {});

        using Callback = decltype(ig_traversal_shader);
        auto callback  = reinterpret_cast<Callback*>(shader_set.SecondaryTraversalShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected secondary traversal shader to be valid");
        setCurrentShader(dev, size, ShaderKey(shader_set.ID, ShaderType::SecondaryTraversal, 0), shader_set.SecondaryTraversalShader);
        callback(&driver_settings, size);

        checkDebugOutput();

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(ShaderType::SecondaryTraversal, {});
    }

    inline int runRayGenerationShader(int32_t dev, int next_id, int size, int xmin, int ymin, int xmax, int ymax)
    {
#ifdef IG_DEBUG_LOG_TRACE
        IG_LOG(L_DEBUG) << "TRACE> Ray Generation Shader [S=" << size << ", I=" << next_id << "]" << std::endl;
#endif

        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::RayGeneration, (xmax - xmin) * (ymax - ymin), {});

        using Callback = decltype(ig_ray_generation_shader);
        auto callback  = reinterpret_cast<Callback*>(shader_set.RayGenerationShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected ray generation shader to be valid");
        setCurrentShader(dev, (xmax - xmin) * (ymax - ymin), ShaderKey(shader_set.ID, ShaderType::RayGeneration, 0), shader_set.RayGenerationShader);
        const int ret = callback(&driver_settings, next_id, size, xmin, ymin, xmax, ymax);

        checkDebugOutput();

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(ShaderType::RayGeneration, {});
        return ret;
    }

    inline void runMissShader(int32_t dev, int first, int last)
    {
#ifdef IG_DEBUG_LOG_TRACE
        IG_LOG(L_DEBUG) << "TRACE> Miss Shader [S=" << first << ", E=" << last << "]" << std::endl;
#endif

        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::Miss, last - first, {});

        using Callback = decltype(ig_miss_shader);
        auto callback  = reinterpret_cast<Callback*>(shader_set.MissShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected miss shader to be valid");
        setCurrentShader(dev, last - first, ShaderKey(shader_set.ID, ShaderType::Miss, 0), shader_set.MissShader);
        callback(&driver_settings, first, last);

        checkDebugOutput();

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(ShaderType::Miss, {});
    }

    inline void runHitShader(int32_t dev, int entity_id, int first, int last)
    {
        const int material_id = database->EntityToMaterial.at(entity_id);

#ifdef IG_DEBUG_LOG_TRACE
        IG_LOG(L_DEBUG) << "TRACE> Hit Shader [I=" << entity_id << ", M=" << material_id << ", S=" << first << ", E=" << last << "]" << std::endl;
#endif

        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(ShaderType::Hit, last - first, material_id);

        using Callback = decltype(ig_hit_shader);
        IG_ASSERT(material_id >= 0 && material_id < (int)shader_set.HitShaders.size(), "Expected material id for hit shaders to be valid");
        const auto& output = shader_set.HitShaders.at(material_id);
        auto callback      = reinterpret_cast<Callback*>(output.Exec);
        IG_ASSERT(callback != nullptr, "Expected hit shader to be valid");
        setCurrentShader(dev, last - first, ShaderKey(shader_set.ID, ShaderType::Hit, (uint32)material_id), output);
        callback(&driver_settings, entity_id, material_id, first, last);

        checkDebugOutput();

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(ShaderType::Hit, material_id);
    }

    inline bool useAdvancedShadowHandling()
    {
        return !shader_set.AdvancedShadowHitShaders.empty() && !shader_set.AdvancedShadowMissShaders.empty();
    }

    inline void runAdvancedShadowShader(int32_t dev, int material_id, int first, int last, bool is_hit)
    {
        IG_ASSERT(useAdvancedShadowHandling(), "Expected advanced shadow shader only be called if it is enabled!");

        if (is_hit) {
#ifdef IG_DEBUG_LOG_TRACE
            IG_LOG(L_DEBUG) << "TRACE> Advanced Hit Shader [I=" << material_id << ", S=" << first << ", E=" << last << "]" << std::endl;
#endif

            if (setup.acquire_stats)
                getThreadData()->stats.beginShaderLaunch(ShaderType::AdvancedShadowHit, last - first, material_id);

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(material_id >= 0 && material_id < (int)shader_set.AdvancedShadowHitShaders.size(), "Expected material id for advanced shadow hit shaders to be valid");
            const auto& output = shader_set.AdvancedShadowHitShaders.at(material_id);
            auto callback      = reinterpret_cast<Callback*>(output.Exec);
            IG_ASSERT(callback != nullptr, "Expected advanced shadow hit shader to be valid");
            setCurrentShader(dev, last - first, ShaderKey(shader_set.ID, ShaderType::AdvancedShadowHit, (uint32)material_id), output);
            callback(&driver_settings, material_id, first, last);

            checkDebugOutput();

            if (setup.acquire_stats)
                getThreadData()->stats.endShaderLaunch(ShaderType::AdvancedShadowHit, material_id);
        } else {
#ifdef IG_DEBUG_LOG_TRACE
            IG_LOG(L_DEBUG) << "TRACE> Advanced Miss Shader [I=" << material_id << ", S=" << first << ", E=" << last << "]" << std::endl;
#endif

            if (setup.acquire_stats)
                getThreadData()->stats.beginShaderLaunch(ShaderType::AdvancedShadowMiss, last - first, material_id);

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(material_id >= 0 && material_id < (int)shader_set.AdvancedShadowMissShaders.size(), "Expected material id for advanced shadow miss shaders to be valid");
            const auto& output = shader_set.AdvancedShadowMissShaders.at(material_id);
            auto callback      = reinterpret_cast<Callback*>(output.Exec);
            IG_ASSERT(callback != nullptr, "Expected advanced shadow miss shader to be valid");
            setCurrentShader(dev, last - first, ShaderKey(shader_set.ID, ShaderType::Miss, (uint32)material_id), output);
            callback(&driver_settings, material_id, first, last);

            checkDebugOutput();

            if (setup.acquire_stats)
                getThreadData()->stats.endShaderLaunch(ShaderType::AdvancedShadowMiss, material_id);
        }
    }

    inline void runCallbackShader(int32_t dev, int type)
    {
        IG_ASSERT(type >= 0 && type < (int)CallbackType::_COUNT, "Expected callback shader type to be well formed!");

        const auto& output = shader_set.CallbackShaders[type];
        using Callback     = decltype(ig_callback_shader);
        auto callback      = reinterpret_cast<Callback*>(output.Exec);

        if (callback != nullptr) {
#ifdef IG_DEBUG_LOG_TRACE
            IG_LOG(L_DEBUG) << "TRACE> Callback Shader [T=" << type << "]" << std::endl;
#endif

            if (setup.acquire_stats)
                getThreadData()->stats.beginShaderLaunch(ShaderType::Callback, 1, type);

            setCurrentShader(dev, 1, ShaderKey(shader_set.ID, ShaderType::Callback, (uint32)type), output);
            callback(&driver_settings);

            checkDebugOutput();

            if (setup.acquire_stats)
                getThreadData()->stats.endShaderLaunch(ShaderType::Callback, type);
        }
    }

    inline float* getFilmImage(int32_t dev)
    {
        if (dev != 0) {
            auto& device = devices[dev];
            if (device.film_pixels.size() != host_pixels.Data.size()) {
                _SECTION(SectionType::FramebufferUpdate);

                auto film_size = film_width * film_height * 3;
                void* ptr      = anydsl_alloc(dev, sizeof(float) * film_size);
                if (ptr == nullptr) {
                    IG_LOG(L_FATAL) << "Out of memory" << std::endl;
                    std::abort();
                    return nullptr;
                }

                auto film_data     = reinterpret_cast<float*>(ptr);
                device.film_pixels = anydsl::Array<float>(dev, film_data, film_size);
                anydsl::copy(host_pixels.Data, device.film_pixels);
            }
            return device.film_pixels.data();
        } else {
            return host_pixels.Data.data();
        }
    }

    inline void mapAOVToDevice(const std::string& name, const AOV& host, bool onlyAtCreation = true)
    {
        int32_t dev = getDevID();
        if (dev == 0) // Device is host
            return;

        bool created = false;
        auto& device = devices[dev];
        if (device.aovs[name].size() != host.Data.size()) {
            _SECTION(SectionType::AOVUpdate);

            auto film_size = film_width * film_height * 3;
            void* ptr      = anydsl_alloc(dev, sizeof(float) * film_size);
            if (ptr == nullptr) {
                IG_LOG(L_FATAL) << "Out of memory" << std::endl;
                std::abort();
            }

            auto film_data    = reinterpret_cast<float*>(ptr);
            device.aovs[name] = anydsl::Array<float>(dev, film_data, film_size);
            created           = true;
        }

        if (!onlyAtCreation || created)
            anydsl::copy(host.Data, device.aovs[name]);
    }

    inline Device::AOVAccessor getAOVImage(const std::string& aov_name)
    {
        const int32_t dev = getDevID();
        if (aov_name.empty() || aov_name == "Color")
            return Device::AOVAccessor{ getFilmImage(dev), host_pixels.IterationCount };

        const auto it = aovs.find(aov_name);
        if (it == aovs.end()) {
            IG_LOG(L_ERROR) << "Unknown aov '" << aov_name << "' access" << std::endl;
            return Device::AOVAccessor{ nullptr, 0 };
        }

        if (dev != 0) {
            auto& device = devices[dev];
            mapAOVToDevice(aov_name, it->second);
            return Device::AOVAccessor{ device.aovs[aov_name].data(), it->second.IterationCount };
        } else {
            return Device::AOVAccessor{ it->second.Data.data(), it->second.IterationCount };
        }
    }

    inline uint32_t* getTonemapImageGPU()
    {
        const int32_t dev      = getDevID();
        const size_t film_size = film_width * film_height;
        auto& device           = devices[dev];
        if ((size_t)device.tonemap_pixels.size() != film_size) {
            _SECTION(SectionType::TonemapUpdate);

            void* ptr = anydsl_alloc(dev, sizeof(uint32_t) * film_size);
            if (ptr == nullptr) {
                IG_LOG(L_FATAL) << "Out of memory" << std::endl;
                std::abort();
                return nullptr;
            }
            auto film_data        = reinterpret_cast<uint32_t*>(ptr);
            device.tonemap_pixels = anydsl::Array<uint32_t>(dev, film_data, film_size);
        }
        return device.tonemap_pixels.data();
    }

    inline Device::AOVAccessor getAOVImageForHost(const std::string& aov_name)
    {
        if (!host_pixels.Data.data()) {
            IG_LOG(L_ERROR) << "Framebuffer not yet initialized. Run a single iteration first" << std::endl;
            return Device::AOVAccessor{ nullptr, 0 };
        }

        if (is_gpu) {
            const int32_t dev = getDevID();
            if (aov_name.empty() || aov_name == "Color") {
                if (!host_pixels.Mapped && devices[dev].film_pixels.data() != nullptr) {
                    _SECTION(SectionType::FramebufferHostUpdate);
                    anydsl::copy(devices[dev].film_pixels, host_pixels.Data);
                    host_pixels.Mapped = true;
                }
                return Device::AOVAccessor{ host_pixels.Data.data(), host_pixels.IterationCount };
            } else {
                const auto it = aovs.find(aov_name);
                if (it == aovs.end()) {
                    IG_LOG(L_ERROR) << "Unknown aov '" << aov_name << "' access for host" << std::endl;
                    return Device::AOVAccessor{ nullptr, 0 };
                }

                if (!it->second.HostOnly && !it->second.Mapped && devices[dev].aovs[aov_name].data() != nullptr) {
                    _SECTION(SectionType::AOVHostUpdate);
                    anydsl::copy(devices[dev].aovs[aov_name], it->second.Data);
                    it->second.Mapped = true;
                }
                return Device::AOVAccessor{ it->second.Data.data(), it->second.IterationCount };
            }
        } else {
            return getAOVImage(aov_name);
        }
    }

    inline void markAOVAsUsed(const char* name, int iter)
    {
        const std::lock_guard<std::mutex> guard(thread_mutex);

        const std::string aov_name = name ? std::string(name) : std::string{};
        if (aov_name.empty() || aov_name == "Color") {
            host_pixels.IterDiff = iter;
        } else {
            const auto it = aovs.find(aov_name);
            if (it == aovs.end())
                IG_LOG(L_ERROR) << "Unknown aov '" << aov_name << "' access" << std::endl;
            else
                it->second.IterDiff = iter;
        }
    }

    /// Clear all aovs and the framebuffer
    inline void clearAllAOVs()
    {
        clearAOV({});
        for (const auto& p : aovs)
            clearAOV(p.first.c_str());
    }

    /// Clear specific aov
    inline void clearAOV(const std::string& aov_name)
    {
        if (!host_pixels.Data.data())
            return;

        if (aov_name.empty() || aov_name == "Color") {
            host_pixels.IterationCount = 0;
            host_pixels.IterDiff       = 0;
            std::memset(host_pixels.Data.data(), 0, sizeof(float) * host_pixels.Data.size());
            for (auto& pair : devices) {
                auto& device_pixels = devices[pair.first].film_pixels;
                if (device_pixels.size() == host_pixels.Data.size())
                    anydsl::copy(host_pixels.Data, device_pixels);
            }
        } else {
            auto& aov          = aovs[aov_name];
            aov.IterationCount = 0;
            aov.IterDiff       = 0;
            auto& buffer       = aov.Data;
            std::memset(buffer.data(), 0, sizeof(float) * buffer.size());
            for (auto& pair : devices) {
                if (devices[pair.first].aovs.empty())
                    continue;
                auto& device_pixels = devices[pair.first].aovs[aov_name];
                if (device_pixels.size() == buffer.size())
                    anydsl::copy(buffer, device_pixels);
            }
        }
    }

#ifdef IG_HAS_DENOISER
    inline void denoise()
    {
        if (aovs.count("Denoised") == 0)
            return;

        const auto color  = getAOVImageForHost({});
        const auto normal = getAOVImageForHost("Normals");
        const auto albedo = getAOVImageForHost("Albedo");
        const auto output = getAOVImageForHost("Denoised");

        IG_ASSERT(color.Data, "Expected valid color data for denoiser");
        IG_ASSERT(normal.Data, "Expected valid normal data for denoiser");
        IG_ASSERT(albedo.Data, "Expected valid albedo data for denoiser");
        IG_ASSERT(output.Data, "Expected valid output data for denoiser");

        ignis_denoise(color.Data, normal.Data, albedo.Data, nullptr, output.Data, film_width, film_height, color.IterationCount);

        // Make sure the iteration count resembles the input
        auto& outputAOV          = aovs["Denoised"];
        outputAOV.IterationCount = color.IterationCount;
        outputAOV.IterDiff       = 0;

        // Map to device
        mapAOVToDevice("Denoised", outputAOV, false);
    }
#endif

    inline void present()
    {
        // Single thread access
        if (!current_settings.info.LockFramebuffer) {
            host_pixels.IterationCount = (size_t)std::max<int64_t>(0, (int64_t)host_pixels.IterationCount + 1);
            host_pixels.IterDiff       = 0;
        }

        for (auto& p : aovs) {
            p.second.IterationCount = (size_t)std::max<int64_t>(0, (int64_t)p.second.IterationCount + p.second.IterDiff);
            p.second.IterDiff       = 0;
        }
    }

    inline Statistics* getFullStats()
    {
        main_stats.reset();
        for (const auto& data : thread_data)
            main_stats.add(data->stats);

        return &main_stats;
    }

    // Access parameters
    int getParameterInt(int32_t dev, const char* name, int def, bool global)
    {
        const ParameterSet* registry = global ? current_parameters : getCurrentLocalRegistry(dev);
        IG_ASSERT(registry != nullptr, "No parameters available!");
        if (registry->IntParameters.count(name) > 0)
            return registry->IntParameters.at(name);
        else
            return def;
    }

    float getParameterFloat(int32_t dev, const char* name, float def, bool global)
    {
        const ParameterSet* registry = global ? current_parameters : getCurrentLocalRegistry(dev);
        IG_ASSERT(registry != nullptr, "No parameters available!");
        if (registry->FloatParameters.count(name) > 0)
            return registry->FloatParameters.at(name);
        else
            return def;
    }

    void getParameterVector(int32_t dev, const char* name, float defX, float defY, float defZ, float& outX, float& outY, float& outZ, bool global)
    {
        const ParameterSet* registry = global ? current_parameters : getCurrentLocalRegistry(dev);
        IG_ASSERT(registry != nullptr, "No parameters available!");
        if (registry->VectorParameters.count(name) > 0) {
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

    void getParameterColor(int32_t dev, const char* name, float defR, float defG, float defB, float defA, float& outR, float& outG, float& outB, float& outA, bool global)
    {
        const ParameterSet* registry = global ? current_parameters : getCurrentLocalRegistry(dev);
        IG_ASSERT(registry != nullptr, "No parameters available!");
        if (registry->ColorParameters.count(name) > 0) {
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
};

static std::unique_ptr<Interface> sInterface;

// --------------------- Device
Device::Device(const Device::SetupSettings& settings)
{
    IG_ASSERT(sInterface == nullptr, "Only a single instance allowed!");
    sInterface = std::make_unique<Interface>(settings);

    IG_LOG(L_INFO) << "Using device " << anydsl_device_name(sInterface->getDevID()) << std::endl;

    // Force flush to zero mode for denormals
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
    _mm_setcsr(_mm_getcsr() | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
#endif
}

Device::~Device()
{
    sInterface.reset();
}

void Device::render(const TechniqueVariantShaderSet& shaderSet, const Device::RenderSettings& settings, const ParameterSet* parameterSet)
{
    // Make sure everything is initialized (lazy setup)
    sInterface->ensureSetup();

    // Register host thread
    sInterface->registerThread();

    sInterface->setupShaderSet(shaderSet);
    sInterface->updateSettings(settings);
    sInterface->current_settings   = settings;
    sInterface->current_parameters = parameterSet;

    sInterface->resetFramebufferAccess();
    sInterface->runDeviceShader();
    sInterface->present();

#ifdef IG_HAS_DENOISER
    if (settings.apply_denoiser)
        sInterface->denoise();
#endif

    sInterface->unregisterThread();
}

void Device::resize(size_t width, size_t height)
{
    sInterface->resizeFramebuffer(width, height);
}

Device::AOVAccessor Device::getFramebuffer(const std::string& name)
{
    return sInterface->getAOVImageForHost(name);
}

void Device::clearAllFramebuffer()
{
    sInterface->clearAllAOVs();
}

void Device::clearFramebuffer(const std::string& name)
{
    sInterface->clearAOV(name);
}

const Statistics* Device::getStatistics()
{
    return sInterface->getFullStats();
}

void Device::tonemap(uint32_t* out_pixels, const TonemapSettings& driver_settings)
{
    // Make sure everything is initialized (lazy setup)
    sInterface->ensureSetup();

    // Register host thread
    sInterface->registerThread();

    const auto acc       = sInterface->getAOVImage(driver_settings.AOV);
    float* in_pixels     = acc.Data;
    const float inv_iter = acc.IterationCount > 0 ? 1.0f / acc.IterationCount : 0.0f;

    uint32_t* device_out_pixels = sInterface->is_gpu ? sInterface->getTonemapImageGPU() : out_pixels;

    ::TonemapSettings settings;
    settings.method          = (int)driver_settings.Method;
    settings.use_gamma       = driver_settings.UseGamma;
    settings.scale           = driver_settings.Scale * inv_iter;
    settings.exposure_factor = driver_settings.ExposureFactor;
    settings.exposure_offset = driver_settings.ExposureOffset;

    sInterface->runTonemapShader(in_pixels, device_out_pixels, settings);

    if (sInterface->is_gpu) {
        size_t size = sInterface->film_width * sInterface->film_height;
        anydsl_copy(sInterface->getDevID(), device_out_pixels, 0, 0 /* Host */, out_pixels, 0, sizeof(uint32_t) * size);
    }

    sInterface->unregisterThread();
}

ImageInfoOutput Device::imageinfo(const ImageInfoSettings& driver_settings)
{
    // Make sure everything is initialized (lazy setup)
    sInterface->ensureSetup();

    // Register host thread
    sInterface->registerThread();

    const auto acc       = sInterface->getAOVImage(driver_settings.AOV);
    float* in_pixels     = acc.Data;
    const float inv_iter = acc.IterationCount > 0 ? 1.0f / acc.IterationCount : 0.0f;

    ::ImageInfoSettings settings;
    settings.scale     = driver_settings.Scale * inv_iter;
    settings.histogram = driver_settings.Histogram;
    settings.bins      = (int)driver_settings.Bins;

    ::ImageInfoOutput output = sInterface->runImageinfoShader(in_pixels, settings);

    ImageInfoOutput driver_output;
    driver_output.Min     = output.min;
    driver_output.Max     = output.max;
    driver_output.Average = output.avg;
    driver_output.SoftMin = output.soft_min;
    driver_output.SoftMax = output.soft_max;
    driver_output.Median  = output.median;

    sInterface->unregisterThread();

    return driver_output;
}

template <typename T>
inline void get_stream(T* dev_stream, DeviceStream& stream, size_t min_components)
{
    static_assert(std::is_pod<T>::value, "Expected stream to be plain old data");
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
IG_EXPORT void ignis_get_film_data(int dev, float** pixels, int* width, int* height)
{
    *pixels = sInterface->getFilmImage(dev);
    *width  = (int)sInterface->film_width;
    *height = (int)sInterface->film_height;
}

IG_EXPORT void ignis_get_aov_image(int dev, const char* name, float** aov_pixels)
{
    IG_UNUSED(dev);
    *aov_pixels = IG::sInterface->getAOVImage(name).Data;
}

IG_EXPORT void ignis_mark_aov_as_used(const char* name, int iter)
{
    sInterface->markAOVAsUsed(name, iter);
}

IG_EXPORT void ignis_get_work_info(WorkInfo* info)
{
    if (sInterface->driver_settings.width > 0 && sInterface->driver_settings.height > 0) {
        info->width  = (int)sInterface->driver_settings.width;
        info->height = (int)sInterface->driver_settings.height;
    } else {
        info->width  = (int)sInterface->film_width;
        info->height = (int)sInterface->film_height;
    }

    info->advanced_shadows                = sInterface->useAdvancedShadowHandling() && sInterface->current_settings.info.ShadowHandlingMode == IG::ShadowHandlingMode::Advanced;
    info->advanced_shadows_with_materials = sInterface->useAdvancedShadowHandling() && sInterface->current_settings.info.ShadowHandlingMode == IG::ShadowHandlingMode::AdvancedWithMaterials;
    info->framebuffer_locked              = sInterface->current_settings.info.LockFramebuffer;
}

IG_EXPORT void ignis_load_bvh2_ent(int dev, const char* prim_type, Node2** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<IG::Bvh2Ent, Node2>(dev, prim_type);
    *nodes    = const_cast<Node2*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

IG_EXPORT void ignis_load_bvh4_ent(int dev, const char* prim_type, Node4** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<IG::Bvh4Ent, Node4>(dev, prim_type);
    *nodes    = const_cast<Node4*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

IG_EXPORT void ignis_load_bvh8_ent(int dev, const char* prim_type, Node8** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<IG::Bvh8Ent, Node8>(dev, prim_type);
    *nodes    = const_cast<Node8*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

IG_EXPORT void ignis_load_scene_info(int dev, SceneInfo* info)
{
    *info = sInterface->loadSceneInfo(dev);
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

IG_EXPORT void ignis_load_dyntable(int dev, const char* name, DynTableData* dtb)
{
    auto& proxy = sInterface->loadDyntable(dev, name);
    *dtb        = assignDynTable(proxy);
}

IG_EXPORT void ignis_load_fixtable(int dev, const char* name, uint8_t** data, int32_t* size)
{
    auto& buf = sInterface->loadFixtable(dev, name);
    *data     = const_cast<uint8_t*>(buf.Data.data());
    *size     = (int32_t)buf.Data.size();
}

IG_EXPORT void ignis_load_rays(int dev, StreamRay** list)
{
    *list = const_cast<StreamRay*>(sInterface->loadRayList(dev).data());
}

IG_EXPORT void ignis_load_image(int32_t dev, const char* file, float** pixels, int32_t* width, int32_t* height, int32_t expected_channels)
{
    auto& img = sInterface->loadImage(dev, file, expected_channels);
    *pixels   = const_cast<float*>(img.Data.data());
    *width    = (int32_t)img.Width;
    *height   = (int32_t)img.Height;
}

IG_EXPORT void ignis_load_image_by_id(int32_t dev, int32_t id, float** pixels, int32_t* width, int32_t* height, int32_t expected_channels)
{
    return ignis_load_image(dev, sInterface->lookupResource(id).c_str(), pixels, width, height, expected_channels);
}

IG_EXPORT void ignis_load_packed_image(int32_t dev, const char* file, uint8_t** pixels, int32_t* width, int32_t* height, int32_t expected_channels, bool linear)
{
    auto& img = sInterface->loadPackedImage(dev, file, expected_channels, linear);
    *pixels   = const_cast<uint8_t*>(img.Data.data());
    *width    = (int32_t)img.Width;
    *height   = (int32_t)img.Height;
}

IG_EXPORT void ignis_load_packed_image_by_id(int32_t dev, int32_t id, uint8_t** pixels, int32_t* width, int32_t* height, int32_t expected_channels, bool linear)
{
    return ignis_load_packed_image(dev, sInterface->lookupResource(id).c_str(), pixels, width, height, expected_channels, linear);
}

IG_EXPORT void ignis_load_buffer(int32_t dev, const char* file, uint8_t** data, int32_t* size)
{
    auto& img = sInterface->loadBuffer(dev, file);
    *data     = const_cast<uint8_t*>(img.Data.data());
    *size     = (int32_t)img.Data.size();
}

IG_EXPORT void ignis_load_buffer_by_id(int32_t dev, int32_t id, uint8_t** data, int32_t* size)
{
    return ignis_load_buffer(dev, sInterface->lookupResource(id).c_str(), data, size);
}

IG_EXPORT void ignis_request_buffer(int32_t dev, const char* name, uint8_t** data, int size, int flags)
{
    auto& buffer = sInterface->requestBuffer(dev, name, size, flags);
    *data        = const_cast<uint8_t*>(buffer.Data.data());
}

IG_EXPORT void ignis_dbg_dump_buffer(int32_t dev, const char* name, const char* filename)
{
    sInterface->dumpBuffer(dev, name, filename);
}

IG_EXPORT void ignis_get_temporary_storage(int dev, TemporaryStorageHost* temp)
{
    const auto& data = sInterface->getTemporaryStorageHost(dev);
    temp->ray_begins = const_cast<int32_t*>(data.ray_begins.data());
    temp->ray_ends   = const_cast<int32_t*>(data.ray_ends.data());
}

IG_EXPORT void ignis_gpu_get_tmp_buffer(int dev, int** buf)
{
    *buf = sInterface->getGPUTemporaryBuffer(dev).data();
}

IG_EXPORT void ignis_get_primary_stream(int dev, int id, PrimaryStream* primary, int size)
{
    auto& stream = sInterface->getPrimaryStream(dev, id, size);
    IG::get_stream(primary, stream, IG::MinPrimaryStreamSize);
}

IG_EXPORT void ignis_get_primary_stream_const(int dev, int id, PrimaryStream* primary)
{
    auto& stream = sInterface->getPrimaryStream(dev, id);
    IG::get_stream(primary, stream, IG::MinPrimaryStreamSize);
}

IG_EXPORT void ignis_get_secondary_stream(int dev, int id, SecondaryStream* secondary, int size)
{
    auto& stream = sInterface->getSecondaryStream(dev, id, size);
    IG::get_stream(secondary, stream, IG::MinSecondaryStreamSize);
}

IG_EXPORT void ignis_get_secondary_stream_const(int dev, int id, SecondaryStream* secondary)
{
    auto& stream = sInterface->getSecondaryStream(dev, id);
    IG::get_stream(secondary, stream, IG::MinSecondaryStreamSize);
}

IG_EXPORT void ignis_gpu_swap_primary_streams(int dev)
{
    sInterface->swapGPUPrimaryStreams(dev);
}

IG_EXPORT void ignis_gpu_swap_secondary_streams(int dev)
{
    sInterface->swapGPUSecondaryStreams(dev);
}

IG_EXPORT void ignis_register_thread()
{
    sInterface->registerThread();
}

IG_EXPORT void ignis_unregister_thread()
{
    sInterface->unregisterThread();
}

IG_EXPORT void ignis_handle_traverse_primary(int dev, int size)
{
    sInterface->runPrimaryTraversalShader(dev, size);
}

IG_EXPORT void ignis_handle_traverse_secondary(int dev, int size)
{
    sInterface->runSecondaryTraversalShader(dev, size);
}

IG_EXPORT int ignis_handle_ray_generation(int dev, int next_id, int size, int xmin, int ymin, int xmax, int ymax)
{
    return sInterface->runRayGenerationShader(dev, next_id, size, xmin, ymin, xmax, ymax);
}

IG_EXPORT void ignis_handle_miss_shader(int dev, int first, int last)
{
    sInterface->runMissShader(dev, first, last);
}

IG_EXPORT void ignis_handle_hit_shader(int dev, int entity_id, int first, int last)
{
    sInterface->runHitShader(dev, entity_id, first, last);
}

IG_EXPORT void ignis_handle_advanced_shadow_shader(int dev, int material_id, int first, int last, bool is_hit)
{
    if (sInterface->current_settings.info.ShadowHandlingMode == IG::ShadowHandlingMode::Advanced)
        sInterface->runAdvancedShadowShader(dev, 0 /* Fix to 0 */, first, last, is_hit);
    else
        sInterface->runAdvancedShadowShader(dev, material_id, first, last, is_hit);
}

IG_EXPORT void ignis_handle_callback_shader(int dev, int type)
{
    sInterface->runCallbackShader(dev, type);
}

// Registry stuff
IG_EXPORT int ignis_get_parameter_i32(int dev, const char* name, int def, bool global)
{
    return sInterface->getParameterInt(dev, name, def, global);
}

IG_EXPORT float ignis_get_parameter_f32(int dev, const char* name, float def, bool global)
{
    return sInterface->getParameterFloat(dev, name, def, global);
}

IG_EXPORT void ignis_get_parameter_vector(int dev, const char* name, float defX, float defY, float defZ, float* x, float* y, float* z, bool global)
{
    sInterface->getParameterVector(dev, name, defX, defY, defZ, *x, *y, *z, global);
}

IG_EXPORT void ignis_get_parameter_color(int dev, const char* name, float defR, float defG, float defB, float defA, float* r, float* g, float* b, float* a, bool global)
{
    sInterface->getParameterColor(dev, name, defR, defG, defB, defA, *r, *g, *b, *a, global);
}

// Stats
IG_EXPORT void ignis_stats_begin_section(int id)
{
    if (!sInterface->setup.acquire_stats)
        return;

    // TODO
    IG_UNUSED(id);
}

IG_EXPORT void ignis_stats_end_section(int id)
{
    if (!sInterface->setup.acquire_stats)
        return;

    // TODO
    IG_UNUSED(id);
}

IG_EXPORT void ignis_stats_add(int id, int value)
{
    if (!sInterface->setup.acquire_stats)
        return;

    sInterface->getThreadData()->stats.increase((IG::Quantity)id, static_cast<uint64_t>(value));
}
}
