#include "Image.h"
#include "Logger.h"
#include "RuntimeStructs.h"
#include "Statistics.h"
#include "config/Build.h"
#include "driver/Interface.h"
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

#if defined(DEVICE_NVVM) || defined(DEVICE_AMD)
#define DEVICE_GPU
#endif

#define _SECTION(type)                                                  \
    const auto sectionClosure = getThreadData()->stats.section((type)); \
    IG_UNUSED(sectionClosure)

static inline size_t roundUp(size_t num, size_t multiple)
{
    if (multiple == 0)
        return num;

    size_t remainder = num % multiple;
    if (remainder == 0)
        return num;

    return num + multiple - remainder;
}

static Settings convert_settings(const DriverRenderSettings& settings, size_t iter, size_t frame)
{
    Settings renderSettings;
    renderSettings.device = (int)settings.device;
    renderSettings.spi    = (int)settings.spi;
    renderSettings.frame  = (int)frame;
    renderSettings.iter   = (int)iter;
    renderSettings.width  = (int)settings.work_width;
    renderSettings.height = (int)settings.work_height;

    return renderSettings;
}

// TODO: This can be improved in the future with a c++ reflection system
// We assume only pointers will be added to the struct below, else the calculation has to be modified.
constexpr size_t MaxRayPayloadComponents = sizeof(decltype(PrimaryStream::user)::e) / sizeof(decltype(PrimaryStream::user)::e[0]);
constexpr size_t RayStreamSize           = sizeof(RayStream) / sizeof(RayStream::id);
constexpr size_t PrimaryStreamSize       = RayStreamSize + MaxRayPayloadComponents + (sizeof(PrimaryStream) - sizeof(PrimaryStream::pad) - sizeof(PrimaryStream::size) - sizeof(PrimaryStream::rays) - sizeof(PrimaryStream::user)) / sizeof(PrimaryStream::ent_id);
constexpr size_t SecondaryStreamSize     = RayStreamSize + (sizeof(SecondaryStream) - sizeof(SecondaryStream::pad) - sizeof(SecondaryStream::size) - sizeof(SecondaryStream::rays)) / sizeof(SecondaryStream::mat_id);

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
    ShallowArray<LookupEntry> LookupEntries;
    ShallowArray<uint8_t> Data;
};

struct SceneDatabaseProxy {
    DynTableProxy Entities;
    DynTableProxy Shapes;
    DynTableProxy BVHs;
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

struct CPUData {
    anydsl::Array<float> cpu_primary;
    anydsl::Array<float> cpu_secondary;
    TemporaryStorageHostProxy temporary_storage_host;
    IG::Statistics stats;
    void* current_shader                           = nullptr;
    const IG::ParameterSet* current_local_registry = nullptr;
    std::unordered_map<void*, ShaderStats> shader_stats;
};
thread_local CPUData* sThreadData = nullptr;

constexpr size_t GPUStreamBufferCount = 2;
class Interface {
    IG_CLASS_NON_COPYABLE(Interface);
    IG_CLASS_NON_MOVEABLE(Interface);

public:
    using DeviceImage       = std::tuple<anydsl::Array<float>, size_t, size_t>;
    using DevicePackedImage = std::tuple<anydsl::Array<uint8_t>, size_t, size_t>; // Packed RGBA
    using DeviceBuffer      = std::tuple<anydsl::Array<uint8_t>, size_t>;

    class DeviceData {
        IG_CLASS_NON_COPYABLE(DeviceData);
        IG_CLASS_NON_MOVEABLE(DeviceData);

    public:
        std::atomic_flag scene_loaded = ATOMIC_FLAG_INIT;
        BvhVariant bvh_ent;
        std::atomic_flag database_loaded = ATOMIC_FLAG_INIT;
        SceneDatabaseProxy database;
        anydsl::Array<int32_t> tmp_buffer;
        TemporaryStorageHostProxy temporary_storage_host;
        std::array<anydsl::Array<float>, GPUStreamBufferCount> primary;
        std::array<anydsl::Array<float>, GPUStreamBufferCount> secondary;
        std::vector<anydsl::Array<float>> aovs;
        anydsl::Array<float> film_pixels;
        anydsl::Array<StreamRay> ray_list;
        std::array<anydsl::Array<float>*, GPUStreamBufferCount> current_primary;
        std::array<anydsl::Array<float>*, GPUStreamBufferCount> current_secondary;
        std::unordered_map<std::string, DeviceImage> images;
        std::unordered_map<std::string, DevicePackedImage> packed_images;
        std::unordered_map<std::string, DeviceBuffer> buffers;
        std::unordered_map<std::string, DynTableProxy> custom_dyntables;

        anydsl::Array<uint32_t> tonemap_pixels;

#ifdef DEVICE_GPU
        void* current_shader                           = nullptr;
        const IG::ParameterSet* current_local_registry = nullptr;
#endif

        inline DeviceData()
            : database()
            , current_primary()
            , current_secondary()
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

#ifndef DEVICE_GPU
    tbb::concurrent_queue<CPUData*> available_thread_data;
#endif
    std::unordered_map<void*, ShaderInfo> shader_infos;

    std::vector<anydsl::Array<float>> aovs;
    anydsl::Array<float> host_pixels;
    const IG::SceneDatabase* database;
    size_t film_width;
    size_t film_height;

    size_t current_iteration;

    DriverRenderSettings current_settings;
    DriverSetupSettings setup;
    const IG::ParameterSet* current_parameters = nullptr;
    IG::TechniqueVariantShaderSet shader_set;

    IG::Statistics main_stats;

    Settings driver_settings;

    inline explicit Interface(const DriverSetupSettings& setup)
        : aovs(setup.aov_count)
        , database(setup.database)
        , film_width(setup.framebuffer_width)
        , film_height(setup.framebuffer_height)
        , current_iteration(0)
        , setup(setup)
        , main_stats()
        , driver_settings()
    {
        // Due to the DLL interface, we do have multiple instances of the logger. Make sure they are the same
        IG_LOGGER = *setup.logger;

        setupFramebuffer();
        setupThreadData();
    }

    inline ~Interface() = default;

    inline const std::string& lookupResource(int32_t id) const
    {
        return setup.resource_map->at(id);
    }

    inline void setupFramebuffer()
    {
        host_pixels = anydsl::Array<float>(film_width * film_height * 3);
        for (auto& arr : aovs)
            arr = anydsl::Array<float>(film_width * film_height * 3);
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

    template <typename T>
    inline anydsl::Array<T>& resizeArray(int32_t dev, anydsl::Array<T>& array, size_t size, size_t multiplier)
    {
        const auto capacity = (size & ~((1 << 5) - 1)) + 32; // round to 32
        if (array.size() < (int64_t)capacity) {
            size_t n  = capacity * multiplier;
            void* ptr = anydsl_alloc(dev, sizeof(T) * n);
            if (ptr == nullptr) {
                IG_LOG(IG::L_FATAL) << "Out of memory" << std::endl;
                std::abort();
            }
            array = std::move(anydsl::Array<T>(dev, reinterpret_cast<T*>(ptr), n));
        }
        return array;
    }

    inline void setupThreadData()
    {
#ifdef DEVICE_GPU
        sThreadData = thread_data.emplace_back(std::make_unique<CPUData>()).get(); // Just one single data available...
#else
        const static size_t max_threads = std::thread::hardware_concurrency() + 1 /* Host */;

        for (size_t t = 0; t < max_threads; ++t) {
            CPUData* ptr = thread_data.emplace_back(std::make_unique<CPUData>()).get();
            available_thread_data.push(ptr);
        }
#endif
    }

    inline void setupShaderSet(const IG::TechniqueVariantShaderSet& shaderSet)
    {
        shader_set = shaderSet;

        // Prepare cache data
        shader_infos[shader_set.RayGenerationShader.Exec] = {};
        shader_infos[shader_set.MissShader.Exec]          = {};
        for (const auto& clb : shader_set.HitShaders)
            shader_infos[clb.Exec] = {};
        for (const auto& clb : shader_set.AdvancedShadowHitShaders)
            shader_infos[clb.Exec] = {};
        for (const auto& clb : shader_set.AdvancedShadowMissShaders)
            shader_infos[clb.Exec] = {};
        for (const auto& clb : shader_set.CallbackShaders)
            shader_infos[clb.Exec] = {};
    }

    inline void registerThread()
    {
#ifndef DEVICE_GPU
        if (sThreadData != nullptr)
            return;

        CPUData* ptr = nullptr;
        while (!available_thread_data.try_pop(ptr))
            std::this_thread::yield();

        if (ptr == nullptr)
            IG_LOG(IG::L_FATAL) << "Registering thread 0x" << std::hex << std::this_thread::get_id() << " failed!" << std::endl;
        else
            sThreadData = ptr;
#endif
    }

    inline void unregisterThread()
    {
#ifndef DEVICE_GPU
        if (sThreadData == nullptr)
            return;

        available_thread_data.push(sThreadData);
        sThreadData = nullptr;
#endif
    }

    inline CPUData* getThreadData()
    {
        IG_ASSERT(sThreadData != nullptr, "Thread not registered");
        return sThreadData;
    }

    inline void setCurrentShader(int32_t dev, int workload, const IG::ShaderOutput<void*>& shader)
    {
#ifdef DEVICE_GPU
        devices[dev].current_shader         = shader.Exec;
        devices[dev].current_local_registry = &shader.LocalRegistry;
        auto data                           = getThreadData();
        data->shader_stats[shader.Exec].call_count++;
        data->shader_stats[shader.Exec].workload_count += (size_t)workload;
#else
        IG_UNUSED(dev);
        auto data                    = getThreadData();
        data->current_shader         = shader.Exec;
        data->current_local_registry = &shader.LocalRegistry;
        data->shader_stats[shader.Exec].call_count++;
        data->shader_stats[shader.Exec].workload_count += (size_t)workload;
#endif
    }

    inline const IG::ParameterSet* getCurrentLocalRegistry(int32_t dev)
    {
#ifdef DEVICE_GPU
        return devices[dev].current_local_registry;
#else
        IG_UNUSED(dev);
        return getThreadData()->current_local_registry;
#endif
    }

    inline ShaderInfo& getCurrentShaderInfo(int32_t dev)
    {
#ifdef DEVICE_GPU
        return shader_infos[devices[dev].current_shader];
#else
        IG_UNUSED(dev);
        return shader_infos[getThreadData()->current_shader];
#endif
    }

    inline anydsl::Array<float>& getCPUPrimaryStream(size_t size)
    {
        return resizeArray(0, getThreadData()->cpu_primary, size, PrimaryStreamSize);
    }

    inline anydsl::Array<float>& getCPUPrimaryStream()
    {
        IG_ASSERT(getThreadData()->cpu_primary.size() > 0, "Expected cpu primary stream to be initialized");
        return getThreadData()->cpu_primary;
    }

    inline anydsl::Array<float>& getCPUSecondaryStream(size_t size)
    {
        return resizeArray(0, getThreadData()->cpu_secondary, size, SecondaryStreamSize);
    }

    inline anydsl::Array<float>& getCPUSecondaryStream()
    {
        IG_ASSERT(getThreadData()->cpu_secondary.size() > 0, "Expected cpu secondary stream to be initialized");
        return getThreadData()->cpu_secondary;
    }

    inline anydsl::Array<float>& getGPUPrimaryStream(int32_t dev, size_t buffer, size_t size)
    {
        return resizeArray(dev, *devices[dev].current_primary.at(buffer), size, PrimaryStreamSize);
    }

    inline anydsl::Array<float>& getGPUPrimaryStream(int32_t dev, size_t buffer)
    {
        IG_ASSERT(devices[dev].current_primary.at(buffer)->size() > 0, "Expected gpu primary stream to be initialized");
        return *devices[dev].current_primary.at(buffer);
    }

    inline anydsl::Array<float>& getGPUSecondaryStream(int32_t dev, size_t buffer, size_t size)
    {
        return resizeArray(dev, *devices[dev].current_secondary.at(buffer), size, SecondaryStreamSize);
    }

    inline anydsl::Array<float>& getGPUSecondaryStream(int32_t dev, size_t buffer)
    {
        IG_ASSERT(devices[dev].current_secondary.at(buffer)->size() > 0, "Expected gpu secondary stream to be initialized");
        return *devices[dev].current_secondary.at(buffer);
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
        return std::max<size_t>(32, std::max(database->EntityTable.entryCount() + 1, database->MaterialCount * 2));
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
    inline const Bvh& loadEntityBVH(int32_t dev)
    {
        auto& device = devices[dev];
        if (!device.scene_loaded.test_and_set())
            device.bvh_ent = std::move(loadSceneBVH<Node>(dev));
        return std::get<Bvh>(device.bvh_ent);
    }

    inline const anydsl::Array<StreamRay>& loadRayList(int32_t dev)
    {
        auto& device = devices[dev];
        if (device.ray_list.size() == (int64_t)film_width)
            return device.ray_list;

        IG_ASSERT(current_settings.rays != nullptr, "Expected list of rays to be available");

        std::vector<StreamRay> rays;
        rays.reserve(film_width);

        for (size_t i = 0; i < film_width; ++i) {
            const auto dRay = current_settings.rays[i];

            float norm = dRay.Direction.norm();
            if (norm < std::numeric_limits<float>::epsilon()) {
                IG_LOG(IG::L_ERROR) << "Invalid ray given: Ray has zero direction!" << std::endl;
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
            IG_LOG(IG::L_FATAL) << "Out of memory" << std::endl;
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

    inline DeviceImage copyToDevice(int32_t dev, const IG::Image& image)
    {
        return DeviceImage(copyToDevice(dev, image.pixels.get(), image.width * image.height * 4), image.width, image.height);
    }

    inline DevicePackedImage copyToDevicePacked(int32_t dev, const IG::Image& image)
    {
        std::vector<uint8_t> packed;
        image.copyToPackedFormat(packed);
        return DevicePackedImage(copyToDevice(dev, packed), image.width, image.height);
    }

    template <typename Node>
    inline BvhProxy<Node, EntityLeaf1> loadSceneBVH(int32_t dev)
    {
        const size_t node_count = database->SceneBVH.Nodes.size() / sizeof(Node);
        const size_t leaf_count = database->SceneBVH.Leaves.size() / sizeof(EntityLeaf1);
        return BvhProxy<Node, EntityLeaf1>{
            std::move(ShallowArray<Node>(dev, reinterpret_cast<const Node*>(database->SceneBVH.Nodes.data()), node_count)),
            std::move(ShallowArray<EntityLeaf1>(dev, reinterpret_cast<const EntityLeaf1*>(database->SceneBVH.Leaves.data()), leaf_count))
        };
    }

    inline DynTableProxy loadDyntable(int32_t dev, const IG::DynTable& tbl)
    {
        static_assert(sizeof(LookupEntry) == sizeof(IG::LookupEntry), "Expected generated Lookup Entry and internal Lookup Entry to be of same size!");

        DynTableProxy proxy;
        proxy.EntryCount    = tbl.entryCount();
        proxy.LookupEntries = ShallowArray<LookupEntry>(dev, (LookupEntry*)tbl.lookups().data(), tbl.lookups().size());
        proxy.Data          = ShallowArray<uint8_t>(dev, tbl.data().data(), tbl.data().size());
        return proxy;
    }

    // Load all the data assembled in previous stages to the device
    inline const SceneDatabaseProxy& loadSceneDatabase(int32_t dev)
    {
        auto& device = devices[dev];
        if (device.database_loaded.test_and_set())
            return device.database;

        SceneDatabaseProxy& proxy = device.database;

        proxy.Entities = loadDyntable(dev, database->EntityTable);
        proxy.Shapes   = loadDyntable(dev, database->ShapeTable);
        proxy.BVHs     = loadDyntable(dev, database->BVHTable);

        return proxy;
    }

    inline SceneInfo loadSceneInfo(int32_t dev)
    {
        IG_UNUSED(dev);

        return SceneInfo{ (int)database->EntityTable.entryCount(),
                          (int)database->MaterialCount };
    }

    inline const DynTableProxy& loadCustomDyntable(int32_t dev, const char* name)
    {
        auto& tables = devices[dev].custom_dyntables;
        auto it      = tables.find(name);
        if (it != tables.end())
            return it->second;

        IG_LOG(IG::L_DEBUG) << "Loading custom dyntable " << name << std::endl;

        return tables[name] = loadDyntable(dev, database->CustomTables.at(name));
    }

    inline const DeviceImage& loadImage(int32_t dev, const std::string& filename, int32_t expected_channels)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& images = devices[dev].images;
        auto it      = images.find(filename);
        if (it != images.end())
            return it->second;

        _SECTION(IG::SectionType::ImageLoading);

        IG_LOG(IG::L_DEBUG) << "Loading image " << filename << " (C=" << expected_channels << ")" << std::endl;
        try {
            const auto img = IG::Image::load(filename);
            if (expected_channels != (int32_t)img.channels) {
                IG_LOG(IG::L_ERROR) << "Image '" << filename << "' is has unexpected channel count" << std::endl;
                return images[filename] = copyToDevice(dev, IG::Image());
            }

            auto& res = getCurrentShaderInfo(dev).images[filename];
            res.counter++;
            res.memory_usage        = img.width * img.height * img.channels * sizeof(float);
            return images[filename] = copyToDevice(dev, img);
        } catch (const IG::ImageLoadException& e) {
            IG_LOG(IG::L_ERROR) << e.what() << std::endl;
            return images[filename] = copyToDevice(dev, IG::Image());
        }
    }

    inline const DevicePackedImage& loadPackedImage(int32_t dev, const std::string& filename, int32_t expected_channels, bool linear)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& images = devices[dev].packed_images;
        auto it      = images.find(filename);
        if (it != images.end())
            return it->second;

        _SECTION(IG::SectionType::PackedImageLoading);

        IG_LOG(IG::L_DEBUG) << "Loading (packed) image " << filename << " (C=" << expected_channels << ")" << std::endl;
        try {
            std::vector<uint8_t> packed;
            size_t width, height, channels;
            IG::Image::loadAsPacked(filename, packed, width, height, channels, linear);

            if (expected_channels != (int32_t)channels) {
                IG_LOG(IG::L_ERROR) << "Packed image '" << filename << "' is has unexpected channel count" << std::endl;
                return images[filename] = DevicePackedImage(copyToDevice(dev, std::vector<uint8_t>{}), 0, 0);
            }

            auto& res = getCurrentShaderInfo(dev).packed_images[filename];
            res.counter++;
            res.memory_usage        = packed.size();
            return images[filename] = DevicePackedImage(copyToDevice(dev, packed), width, height);
        } catch (const IG::ImageLoadException& e) {
            IG_LOG(IG::L_ERROR) << e.what() << std::endl;
            return images[filename] = DevicePackedImage(copyToDevice(dev, std::vector<uint8_t>{}), 0, 0);
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

        _SECTION(IG::SectionType::BufferLoading);

        IG_LOG(IG::L_DEBUG) << "Loading buffer " << filename << std::endl;
        const auto vec = readBufferFile(filename);

        if ((vec.size() % sizeof(int32_t)) != 0)
            IG_LOG(IG::L_WARNING) << "Buffer " << filename << " is not properly sized!" << std::endl;

        return buffers[filename] = DeviceBuffer(copyToDevice(dev, vec), vec.size());
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
        if (it != buffers.end() && std::get<1>(it->second) >= (size_t)size) {
            return it->second;
        }

        _SECTION(IG::SectionType::BufferRequests);

        IG_LOG(IG::L_DEBUG) << "Requested buffer " << name << " with " << size << " bytes" << std::endl;

        void* ptr = anydsl_alloc(dev, size);
        if (ptr == nullptr) {
            IG_LOG(IG::L_FATAL) << "Out of memory" << std::endl;
            std::abort();
        }

        buffers[name] = DeviceBuffer(anydsl::Array<uint8_t>(dev, reinterpret_cast<uint8_t*>(ptr), size), size);

        return buffers[name];
    }

    inline void dumpBuffer(int32_t dev, const std::string& name, const std::string& filename)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& buffers = devices[dev].buffers;
        auto it       = buffers.find(name);
        if (it != buffers.end()) {
            const size_t size = std::get<1>(it->second);

            // Copy data to host
            std::vector<uint8_t> host_data(size);
            anydsl_copy(dev, std::get<0>(it->second).data(), 0, 0 /* Host */, host_data.data(), 0, host_data.size());

            // Dump data as binary glob
            std::ofstream out(filename);
            out.write(reinterpret_cast<const char*>(host_data.data()), host_data.size());
            out.close();
        } else {
            IG_LOG(IG::L_WARNING) << "Buffer " << name << " can not be dumped as it does not exists" << std::endl;
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
            std::vector<uint8_t> host_data(std::get<1>(buffer));
            anydsl_copy(dev, std::get<0>(buffer).data(), 0, 0 /* Host */, host_data.data(), 0, host_data.size());

            // Parse data
            int32_t* ptr  = reinterpret_cast<int32_t*>(host_data.data());
            int32_t occup = std::min(ptr[0], static_cast<int32_t>(host_data.size() / sizeof(int32_t)));

            if (occup <= 0)
                return;

            handleDebug(ptr, occup);

            // Copy back to device
            anydsl_copy(0 /* Host */, host_data.data(), 0, dev, std::get<0>(buffer).data(), 0, sizeof(int32_t));
        } else {
            // Already on the host
            int32_t* ptr  = reinterpret_cast<int32_t*>(std::get<0>(buffer).data());
            int32_t occup = std::min(ptr[0], static_cast<int32_t>(std::get<1>(buffer) / sizeof(int32_t)));

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

    inline int runRayGenerationShader(int32_t dev, int* id, int size, int xmin, int ymin, int xmax, int ymax)
    {
        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(IG::ShaderType::RayGeneration, (xmax - xmin) * (ymax - ymin), {});

        using Callback = decltype(ig_ray_generation_shader);
        auto callback  = reinterpret_cast<Callback*>(shader_set.RayGenerationShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected ray generation shader to be valid");
        setCurrentShader(dev, (xmax - xmin) * (ymax - ymin), shader_set.RayGenerationShader);
        const int ret = callback(&driver_settings, (int)current_iteration, id, size, xmin, ymin, xmax, ymax);

        checkDebugOutput();

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(IG::ShaderType::RayGeneration, {});
        return ret;
    }

    inline void runMissShader(int32_t dev, int first, int last)
    {
        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(IG::ShaderType::Miss, last - first, {});

        using Callback = decltype(ig_miss_shader);
        auto callback  = reinterpret_cast<Callback*>(shader_set.MissShader.Exec);
        IG_ASSERT(callback != nullptr, "Expected miss shader to be valid");
        setCurrentShader(dev, last - first, shader_set.MissShader);
        callback(&driver_settings, first, last);

        checkDebugOutput();

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(IG::ShaderType::Miss, {});
    }

    inline void runHitShader(int32_t dev, int entity_id, int first, int last)
    {
        const int material_id = database->EntityToMaterial.at(entity_id);

        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(IG::ShaderType::Hit, last - first, material_id);

        using Callback = decltype(ig_hit_shader);
        IG_ASSERT(material_id >= 0 && material_id < (int)shader_set.HitShaders.size(), "Expected material id for hit shaders to be valid");
        const auto& output = shader_set.HitShaders.at(material_id);
        auto callback      = reinterpret_cast<Callback*>(output.Exec);
        IG_ASSERT(callback != nullptr, "Expected hit shader to be valid");
        setCurrentShader(dev, last - first, output);
        callback(&driver_settings, entity_id, material_id, first, last);

        checkDebugOutput();

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(IG::ShaderType::Hit, material_id);
    }

    inline bool useAdvancedShadowHandling()
    {
        return !shader_set.AdvancedShadowHitShaders.empty() && !shader_set.AdvancedShadowMissShaders.empty();
    }

    inline void runAdvancedShadowShader(int32_t dev, int material_id, int first, int last, bool is_hit)
    {
        IG_ASSERT(useAdvancedShadowHandling(), "Expected advanced shadow shader only be called if it is enabled!");

        if (is_hit) {
            if (setup.acquire_stats)
                getThreadData()->stats.beginShaderLaunch(IG::ShaderType::AdvancedShadowHit, last - first, material_id);

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(material_id >= 0 && material_id < (int)shader_set.AdvancedShadowHitShaders.size(), "Expected material id for advanced shadow hit shaders to be valid");
            const auto& output = shader_set.AdvancedShadowHitShaders.at(material_id);
            auto callback      = reinterpret_cast<Callback*>(output.Exec);
            IG_ASSERT(callback != nullptr, "Expected advanced shadow hit shader to be valid");
            setCurrentShader(dev, last - first, output);
            callback(&driver_settings, material_id, first, last);

            checkDebugOutput();

            if (setup.acquire_stats)
                getThreadData()->stats.endShaderLaunch(IG::ShaderType::AdvancedShadowHit, material_id);
        } else {
            if (setup.acquire_stats)
                getThreadData()->stats.beginShaderLaunch(IG::ShaderType::AdvancedShadowMiss, last - first, material_id);

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(material_id >= 0 && material_id < (int)shader_set.AdvancedShadowMissShaders.size(), "Expected material id for advanced shadow miss shaders to be valid");
            const auto& output = shader_set.AdvancedShadowMissShaders.at(material_id);
            auto callback      = reinterpret_cast<Callback*>(output.Exec);
            IG_ASSERT(callback != nullptr, "Expected advanced shadow miss shader to be valid");
            setCurrentShader(dev, last - first, output);
            callback(&driver_settings, material_id, first, last);

            checkDebugOutput();

            if (setup.acquire_stats)
                getThreadData()->stats.endShaderLaunch(IG::ShaderType::AdvancedShadowMiss, material_id);
        }
    }

    inline void runCallbackShader(int32_t dev, int type)
    {
        IG_ASSERT(type >= 0 && type < (int)IG::CallbackType::_COUNT, "Expected callback shader type to be well formed!");

        const auto& output = shader_set.CallbackShaders[type];
        using Callback     = decltype(ig_callback_shader);
        auto callback      = reinterpret_cast<Callback*>(output.Exec);

        if (callback != nullptr) {
            if (setup.acquire_stats)
                getThreadData()->stats.beginShaderLaunch(IG::ShaderType::Callback, 1, type);

            setCurrentShader(dev, 1, output);
            callback(&driver_settings, (int)current_iteration);

            checkDebugOutput();

            if (setup.acquire_stats)
                getThreadData()->stats.endShaderLaunch(IG::ShaderType::Callback, type);
        }
    }

    inline float* getFilmImage(int32_t dev)
    {
        if (dev != 0) {
            auto& device = devices[dev];
            if (device.film_pixels.size() != host_pixels.size()) {
                _SECTION(IG::SectionType::FramebufferUpdate);

                auto film_size = film_width * film_height * 3;
                void* ptr      = anydsl_alloc(dev, sizeof(float) * film_size);
                if (ptr == nullptr) {
                    IG_LOG(IG::L_FATAL) << "Out of memory" << std::endl;
                    std::abort();
                    return nullptr;
                }

                auto film_data     = reinterpret_cast<float*>(ptr);
                device.film_pixels = anydsl::Array<float>(dev, film_data, film_size);
                anydsl::copy(host_pixels, device.film_pixels);
            }
            return device.film_pixels.data();
        } else {
            return host_pixels.data();
        }
    }

    inline float* getAOVImage(int32_t dev, int32_t id)
    {
        if (id == 0) // Id = 0 is the actual framebuffer
            return getFilmImage(dev);

        int32_t index = id - 1;
        IG_ASSERT(index < (int32_t)aovs.size(), "AOV index out of bounds!");

        if (dev != 0) {
            auto& device = devices[dev];
            if (device.aovs.size() != aovs.size())
                device.aovs.resize(aovs.size());

            if (device.aovs[index].size() != aovs[index].size()) {
                _SECTION(IG::SectionType::AOVUpdate);

                auto film_size = film_width * film_height * 3;
                void* ptr      = anydsl_alloc(dev, sizeof(float) * film_size);
                if (ptr == nullptr) {
                    IG_LOG(IG::L_FATAL) << "Out of memory" << std::endl;
                    std::abort();
                    return nullptr;
                }

                auto film_data     = reinterpret_cast<float*>(ptr);
                device.aovs[index] = anydsl::Array<float>(dev, film_data, film_size);
                anydsl::copy(aovs[index], device.aovs[index]);
            }
            return device.aovs[index].data();
        } else {
            return aovs[index].data();
        }
    }

#ifdef DEVICE_GPU
    inline uint32_t* getTonemapImage(int32_t dev)
    {
        const size_t film_size = film_width * film_height;
        auto& device           = devices[dev];
        if ((size_t)device.tonemap_pixels.size() != film_size) {
            _SECTION(IG::SectionType::TonemapUpdate);

            void* ptr = anydsl_alloc(dev, sizeof(uint32_t) * film_size);
            if (ptr == nullptr) {
                IG_LOG(IG::L_FATAL) << "Out of memory" << std::endl;
                std::abort();
                return nullptr;
            }
            auto film_data        = reinterpret_cast<uint32_t*>(ptr);
            device.tonemap_pixels = anydsl::Array<uint32_t>(dev, film_data, film_size);
        }
        return device.tonemap_pixels.data();
    }
#endif

    inline void present(int32_t dev)
    {
        for (size_t id = 0; id < devices[dev].aovs.size(); ++id)
            anydsl::copy(devices[dev].aovs[id], aovs[id]);

        anydsl::copy(devices[dev].film_pixels, host_pixels);
    }

    /// Clear specific aov or clear all if aov < 0. Note, aov == 0 is the framebuffer
    inline void clear(int aov)
    {
        if (aov <= 0) {
            std::memset(host_pixels.data(), 0, sizeof(float) * host_pixels.size());
            for (auto& pair : devices) {
                auto& device_pixels = devices[pair.first].film_pixels;
                if (device_pixels.size() == host_pixels.size())
                    anydsl::copy(host_pixels, device_pixels);
            }
        }

        for (size_t id = 0; id < aovs.size(); ++id) {
            if (aov < 0 || (size_t)aov == (id + 1)) {
                auto& buffer = aovs[id];
                std::memset(buffer.data(), 0, sizeof(float) * buffer.size());
                for (auto& pair : devices) {
                    if (devices[pair.first].aovs.empty())
                        continue;
                    auto& device_pixels = devices[pair.first].aovs[id];
                    if (device_pixels.size() == buffer.size())
                        anydsl::copy(buffer, device_pixels);
                }
            }
        }
    }

    inline IG::Statistics* getFullStats()
    {
        main_stats.reset();
        for (const auto& data : thread_data)
            main_stats.add(data->stats);

        return &main_stats;
    }

    // Access parameters
    int getParameterInt(int32_t dev, const char* name, int def, bool global)
    {
        const IG::ParameterSet* registry = global ? current_parameters : getCurrentLocalRegistry(dev);
        IG_ASSERT(registry != nullptr, "No parameters available!");
        if (registry->IntParameters.count(name) > 0)
            return registry->IntParameters.at(name);
        else
            return def;
    }

    float getParameterFloat(int32_t dev, const char* name, float def, bool global)
    {
        const IG::ParameterSet* registry = global ? current_parameters : getCurrentLocalRegistry(dev);
        IG_ASSERT(registry != nullptr, "No parameters available!");
        if (registry->FloatParameters.count(name) > 0)
            return registry->FloatParameters.at(name);
        else
            return def;
    }

    void getParameterVector(int32_t dev, const char* name, float defX, float defY, float defZ, float& outX, float& outY, float& outZ, bool global)
    {
        const IG::ParameterSet* registry = global ? current_parameters : getCurrentLocalRegistry(dev);
        IG_ASSERT(registry != nullptr, "No parameters available!");
        if (registry->VectorParameters.count(name) > 0) {
            IG::Vector3f param = registry->VectorParameters.at(name);
            outX               = param.x();
            outY               = param.y();
            outZ               = param.z();
        } else {
            outX = defX;
            outY = defY;
            outZ = defZ;
        }
    }

    void getParameterColor(int32_t dev, const char* name, float defR, float defG, float defB, float defA, float& outR, float& outG, float& outB, float& outA, bool global)
    {
        const IG::ParameterSet* registry = global ? current_parameters : getCurrentLocalRegistry(dev);
        IG_ASSERT(registry != nullptr, "No parameters available!");
        if (registry->ColorParameters.count(name) > 0) {
            IG::Vector4f param = registry->ColorParameters.at(name);
            outR               = param.x();
            outG               = param.y();
            outB               = param.z();
            outA               = param.w();
        } else {
            outR = defR;
            outG = defG;
            outB = defB;
            outA = defA;
        }
    }
};

static std::unique_ptr<Interface> sInterface;

void glue_render(const IG::TechniqueVariantShaderSet& shaderSet, const DriverRenderSettings& settings, const IG::ParameterSet* parameterSet, size_t iter, size_t frame)
{
    // Register host thread
    sInterface->registerThread();

    sInterface->setupShaderSet(shaderSet);
    sInterface->current_iteration  = iter;
    sInterface->current_settings   = settings;
    sInterface->current_parameters = parameterSet;
    sInterface->driver_settings    = convert_settings(settings, iter, frame);

    CPUData* cpu_data = nullptr;
    if (sInterface->setup.acquire_stats) {
        cpu_data = sInterface->getThreadData();
        cpu_data->stats.beginShaderLaunch(IG::ShaderType::Device, 1, {});
    }

    ig_render(&sInterface->driver_settings);

    if (sInterface->setup.acquire_stats)
        cpu_data->stats.endShaderLaunch(IG::ShaderType::Device, {});

    sInterface->unregisterThread();
}

void glue_setup(const DriverSetupSettings& settings)
{
    IG_ASSERT(sInterface == nullptr, "Only a single instance allowed!");
    sInterface = std::make_unique<Interface>(settings);

    // Force flush to zero mode for denormals
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
    _mm_setcsr(_mm_getcsr() | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
#endif

    // Make sure the functions exposed are available in the linking process
    anydsl_link(settings.driver_filename);
}

void glue_shutdown()
{
    sInterface.reset();
}

void glue_resizeFramebuffer(size_t width, size_t height)
{
    sInterface->resizeFramebuffer(width, height);
}

const float* glue_getFramebuffer(size_t aov)
{
    if (aov == 0 || (size_t)aov > sInterface->aovs.size())
        return sInterface->host_pixels.data();
    else
        return sInterface->aovs[aov - 1].data();
}

void glue_clearFramebuffer(int aov)
{
    sInterface->clear(aov);
}

const IG::Statistics* glue_getStatistics()
{
    return sInterface->getFullStats();
}

static inline int get_dev_id(size_t device)
{
#if defined(DEVICE_NVVM)
    return ANYDSL_DEVICE(ANYDSL_CUDA, (int)device);
#elif defined(DEVICE_AMD)
    return ANYDSL_DEVICE(ANYDSL_HSA, (int)device);
#else
    IG_UNUSED(device);
    return 0;
#endif
}

void glue_tonemap(size_t device, uint32_t* out_pixels, const IG::TonemapSettings& driver_settings)
{
    // Register host thread
    sInterface->registerThread();

    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.beginShaderLaunch(IG::ShaderType::Tonemap, 1, {});

    int dev_id       = get_dev_id(device);
    float* in_pixels = sInterface->getAOVImage(dev_id, (int)driver_settings.AOV);

#ifdef DEVICE_GPU
    uint32_t* device_out_pixels = sInterface->getTonemapImage(dev_id);
#else
    uint32_t* device_out_pixels = out_pixels;
#endif

    TonemapSettings settings;
    settings.method          = (int)driver_settings.Method;
    settings.use_gamma       = driver_settings.UseGamma;
    settings.scale           = driver_settings.Scale;
    settings.exposure_factor = driver_settings.ExposureFactor;
    settings.exposure_offset = driver_settings.ExposureOffset;

    ig_utility_tonemap((int)device, in_pixels, device_out_pixels, (int)sInterface->film_width, (int)sInterface->film_height, &settings);

#ifdef DEVICE_GPU
    size_t size = sInterface->film_width * sInterface->film_height;
    anydsl_copy(dev_id, device_out_pixels, 0, 0 /* Host */, out_pixels, 0, sizeof(uint32_t) * size);
#endif

    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.endShaderLaunch(IG::ShaderType::Tonemap, {});

    sInterface->unregisterThread();
}

void glue_imageinfo(size_t device, const IG::ImageInfoSettings& driver_settings, IG::ImageInfoOutput& driver_output)
{
    // Register host thread
    sInterface->registerThread();

    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.beginShaderLaunch(IG::ShaderType::ImageInfo, 1, {});

    int dev_id       = get_dev_id(device);
    float* in_pixels = sInterface->getAOVImage(dev_id, (int)driver_settings.AOV);

    ImageInfoSettings settings;
    settings.scale     = driver_settings.Scale;
    settings.histogram = driver_settings.Histogram;
    settings.bins      = (int)driver_settings.Bins;

    ImageInfoOutput output;
    ig_utility_imageinfo((int)device, in_pixels, (int)sInterface->film_width, (int)sInterface->film_height, &settings, &output);

    driver_output.Min     = output.min;
    driver_output.Max     = output.max;
    driver_output.Average = output.avg;
    driver_output.SoftMin = output.soft_min;
    driver_output.SoftMax = output.soft_max;
    driver_output.Median  = output.median;

    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.endShaderLaunch(IG::ShaderType::ImageInfo, {});

    sInterface->unregisterThread();
}

inline void get_ray_stream(RayStream& rays, float* ptr, size_t capacity)
{
    static_assert(std::is_pod<RayStream>::value, "Expected RayStream to be plain old data");

    auto r_ptr = reinterpret_cast<float**>(&rays);
    for (size_t i = 0; i < RayStreamSize; ++i)
        r_ptr[i] = ptr + i * capacity;
}

inline void get_primary_stream(PrimaryStream& primary, float* ptr, size_t capacity, size_t components)
{
    static_assert(std::is_pod<PrimaryStream>::value, "Expected PrimaryStream to be plain old data");

    get_ray_stream(primary.rays, ptr, capacity);

    auto r_ptr = reinterpret_cast<float**>(&primary);
    for (size_t i = RayStreamSize; i < components; ++i)
        r_ptr[i] = ptr + i * capacity;

    primary.size = 0;
}

inline void get_secondary_stream(SecondaryStream& secondary, float* ptr, size_t capacity)
{
    static_assert(std::is_pod<SecondaryStream>::value, "Expected SecondaryStream to be plain old data");

    get_ray_stream(secondary.rays, ptr, capacity);

    auto r_ptr = reinterpret_cast<float**>(&secondary);
    for (size_t i = RayStreamSize; i < SecondaryStreamSize; ++i)
        r_ptr[i] = ptr + i * capacity;

    secondary.size = 0;
}

// TODO: Really fixed at compile time?
#ifdef IG_DEBUG
constexpr uint32_t OPT_LEVEL = 0;
#else
constexpr uint32_t OPT_LEVEL = 3;
#endif

void* glue_compileSource(const char* src, const char* function, bool isVerbose)
{
#ifdef IG_DEBUG
    anydsl_set_log_level(isVerbose ? 1 /* info */ : 4 /* error */);
#else
    anydsl_set_log_level(isVerbose ? 3 /* warn */ : 4 /* error */);
#endif

    size_t len = std::strlen(src);
    int ret    = anydsl_compile(src, (uint32_t)len, OPT_LEVEL);
    if (ret < 0)
        return nullptr;

    return anydsl_lookup_function(ret, function);
}

extern "C" {
IG_EXPORT DriverInterface ig_get_interface()
{
    DriverInterface interface {
    };
    interface.MajorVersion = IG::Build::getVersion().Major;
    interface.MinorVersion = IG::Build::getVersion().Minor;
    interface.Revision     = IG::Build::getGitRevision();

// Expose Target
#if defined(DEVICE_DEFAULT)
    interface.Target = IG::Target::GENERIC;
#elif defined(DEVICE_SINGLE)
    interface.Target = IG::Target::SINGLE;
#elif defined(DEVICE_AVX)
    interface.Target = IG::Target::AVX;
#elif defined(DEVICE_AVX2)
    interface.Target = IG::Target::AVX2;
#elif defined(DEVICE_AVX512)
    interface.Target = IG::Target::AVX512;
#elif defined(DEVICE_SSE42)
    interface.Target = IG::Target::SSE42;
#elif defined(DEVICE_ASIMD)
    interface.Target = IG::Target::ASIMD;
#elif defined(DEVICE_NVVM)
    interface.Target = IG::Target::NVVM;
#elif defined(DEVICE_AMD)
    interface.Target = IG::Target::AMDGPU;
#else
#error No device selected!
#endif

    interface.SetupFunction             = glue_setup;
    interface.ShutdownFunction          = glue_shutdown;
    interface.RenderFunction            = glue_render;
    interface.ResizeFramebufferFunction = glue_resizeFramebuffer;
    interface.GetFramebufferFunction    = glue_getFramebuffer;
    interface.ClearFramebufferFunction  = glue_clearFramebuffer;
    interface.GetStatisticsFunction     = glue_getStatistics;
    interface.TonemapFunction           = glue_tonemap;
    interface.ImageInfoFunction         = glue_imageinfo;
    interface.CompileSourceFunction     = glue_compileSource;

    return interface;
}

IG_EXPORT void ignis_get_film_data(int dev, float** pixels, int* width, int* height)
{
    *pixels = sInterface->getFilmImage(dev);
    *width  = (int)sInterface->film_width;
    *height = (int)sInterface->film_height;
}

IG_EXPORT void ignis_get_aov_image(int dev, int id, float** aov_pixels)
{
    *aov_pixels = sInterface->getAOVImage(dev, id);
}

IG_EXPORT void ignis_get_work_info(WorkInfo* info)
{
    if (sInterface->current_settings.work_width > 0 && sInterface->current_settings.work_height > 0) {
        info->width  = (int)sInterface->current_settings.work_width;
        info->height = (int)sInterface->current_settings.work_height;
    } else {
        info->width  = (int)sInterface->film_width;
        info->height = (int)sInterface->film_height;
    }

    info->advanced_shadows                = sInterface->useAdvancedShadowHandling() && sInterface->current_settings.info.ShadowHandlingMode == IG::ShadowHandlingMode::Advanced;
    info->advanced_shadows_with_materials = sInterface->useAdvancedShadowHandling() && sInterface->current_settings.info.ShadowHandlingMode == IG::ShadowHandlingMode::AdvancedWithMaterials;
    info->framebuffer_locked              = sInterface->current_settings.info.LockFramebuffer;
}

IG_EXPORT void ignis_load_bvh2_ent(int dev, Node2** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<Bvh2Ent, Node2>(dev);
    *nodes    = const_cast<Node2*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

IG_EXPORT void ignis_load_bvh4_ent(int dev, Node4** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<Bvh4Ent, Node4>(dev);
    *nodes    = const_cast<Node4*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

IG_EXPORT void ignis_load_bvh8_ent(int dev, Node8** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<Bvh8Ent, Node8>(dev);
    *nodes    = const_cast<Node8*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

IG_EXPORT void ignis_load_scene(int dev, SceneDatabase* dtb)
{
    auto assign = [&](const DynTableProxy& tbl) {
        DynTable devtbl;
        devtbl.count  = tbl.EntryCount;
        devtbl.header = const_cast<LookupEntry*>(tbl.LookupEntries.ptr());
        uint64_t size = tbl.Data.size();
        devtbl.size   = size;
        devtbl.start  = const_cast<uint8_t*>(tbl.Data.ptr());
        return devtbl;
    };

    auto& proxy   = sInterface->loadSceneDatabase(dev);
    dtb->entities = assign(proxy.Entities);
    dtb->shapes   = assign(proxy.Shapes);
    dtb->bvhs     = assign(proxy.BVHs);
}

IG_EXPORT void ignis_load_scene_info(int dev, SceneInfo* info)
{
    *info = sInterface->loadSceneInfo(dev);
}

IG_EXPORT void ignis_load_custom_dyntable(int dev, const char* name, DynTable* dtb)
{
    auto assign = [&](const DynTableProxy& tbl) {
        DynTable devtbl;
        devtbl.count  = tbl.EntryCount;
        devtbl.header = const_cast<LookupEntry*>(tbl.LookupEntries.ptr());
        uint64_t size = tbl.Data.size();
        devtbl.size   = size;
        devtbl.start  = const_cast<uint8_t*>(tbl.Data.ptr());
        return devtbl;
    };

    auto& proxy = sInterface->loadCustomDyntable(dev, name);
    *dtb        = assign(proxy);
}

IG_EXPORT void ignis_load_rays(int dev, StreamRay** list)
{
    *list = const_cast<StreamRay*>(sInterface->loadRayList(dev).data());
}

IG_EXPORT void ignis_load_image(int32_t dev, const char* file, float** pixels, int32_t* width, int32_t* height, int32_t expected_channels)
{
    auto& img = sInterface->loadImage(dev, file, expected_channels);
    *pixels   = const_cast<float*>(std::get<0>(img).data());
    *width    = (int)std::get<1>(img);
    *height   = (int)std::get<2>(img);
}

IG_EXPORT void ignis_load_image_by_id(int32_t dev, int32_t id, float** pixels, int32_t* width, int32_t* height, int32_t expected_channels)
{
    return ignis_load_image(dev, sInterface->lookupResource(id).c_str(), pixels, width, height, expected_channels);
}

IG_EXPORT void ignis_load_packed_image(int32_t dev, const char* file, uint8_t** pixels, int32_t* width, int32_t* height, int32_t expected_channels, bool linear)
{
    auto& img = sInterface->loadPackedImage(dev, file, expected_channels, linear);
    *pixels   = const_cast<uint8_t*>(std::get<0>(img).data());
    *width    = (int)std::get<1>(img);
    *height   = (int)std::get<2>(img);
}

IG_EXPORT void ignis_load_packed_image_by_id(int32_t dev, int32_t id, uint8_t** pixels, int32_t* width, int32_t* height, int32_t expected_channels, bool linear)
{
    return ignis_load_packed_image(dev, sInterface->lookupResource(id).c_str(), pixels, width, height, expected_channels, linear);
}

IG_EXPORT void ignis_load_buffer(int32_t dev, const char* file, uint8_t** data, int32_t* size)
{
    auto& img = sInterface->loadBuffer(dev, file);
    *data     = const_cast<uint8_t*>(std::get<0>(img).data());
    *size     = (int)std::get<1>(img);
}

IG_EXPORT void ignis_load_buffer_by_id(int32_t dev, int32_t id, uint8_t** data, int32_t* size)
{
    return ignis_load_buffer(dev, sInterface->lookupResource(id).c_str(), data, size);
}

IG_EXPORT void ignis_request_buffer(int32_t dev, const char* name, uint8_t** data, int size, int flags)
{
    auto& buffer = sInterface->requestBuffer(dev, name, size, flags);
    *data        = const_cast<uint8_t*>(std::get<0>(buffer).data());
}

IG_EXPORT void ignis_dbg_dump_buffer(int32_t dev, const char* name, const char* filename)
{
    sInterface->dumpBuffer(dev, name, filename);
}

IG_EXPORT void ignis_cpu_get_primary_stream(PrimaryStream* primary, int size)
{
    auto& array = sInterface->getCPUPrimaryStream(size);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

IG_EXPORT void ignis_cpu_get_primary_stream_const(PrimaryStream* primary)
{
    auto& array = sInterface->getCPUPrimaryStream();
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

IG_EXPORT void ignis_cpu_get_secondary_stream(SecondaryStream* secondary, int size)
{
    auto& array = sInterface->getCPUSecondaryStream(size);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

IG_EXPORT void ignis_cpu_get_secondary_stream_const(SecondaryStream* secondary)
{
    auto& array = sInterface->getCPUSecondaryStream();
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
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

IG_EXPORT void ignis_gpu_get_first_primary_stream(int dev, PrimaryStream* primary, int size)
{
    auto& array = sInterface->getGPUPrimaryStream(dev, 0, size);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

IG_EXPORT void ignis_gpu_get_first_primary_stream_const(int dev, PrimaryStream* primary)
{
    auto& array = sInterface->getGPUPrimaryStream(dev, 0);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

IG_EXPORT void ignis_gpu_get_second_primary_stream(int dev, PrimaryStream* primary, int size)
{
    auto& array = sInterface->getGPUPrimaryStream(dev, 1, size);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

IG_EXPORT void ignis_gpu_get_second_primary_stream_const(int dev, PrimaryStream* primary)
{
    auto& array = sInterface->getGPUPrimaryStream(dev, 1);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

IG_EXPORT void ignis_gpu_get_first_secondary_stream(int dev, SecondaryStream* secondary, int size)
{
    auto& array = sInterface->getGPUSecondaryStream(dev, 0, size);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

IG_EXPORT void ignis_gpu_get_first_secondary_stream_const(int dev, SecondaryStream* secondary)
{
    auto& array = sInterface->getGPUSecondaryStream(dev, 0);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

IG_EXPORT void ignis_gpu_get_second_secondary_stream(int dev, SecondaryStream* secondary, int size)
{
    auto& array = sInterface->getGPUSecondaryStream(dev, 1, size);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

IG_EXPORT void ignis_gpu_get_second_secondary_stream_const(int dev, SecondaryStream* secondary)
{
    auto& array = sInterface->getGPUSecondaryStream(dev, 1);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
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

IG_EXPORT int ignis_handle_ray_generation(int dev, int* id, int size, int xmin, int ymin, int xmax, int ymax)
{
    return sInterface->runRayGenerationShader(dev, id, size, xmin, ymin, xmax, ymax);
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

IG_EXPORT void ignis_present(int dev)
{
    if (dev != 0)
        sInterface->present(dev);
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
