#include "Image.h"
#include "Logger.h"
#include "Runtime.h"
#include "Statistics.h"
#include "config/Version.h"
#include "driver/Interface.h"
#include "table/SceneDatabase.h"

#include "generated_interface.h"

#include "ShallowArray.h"

#include <anydsl_runtime.hpp>

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

#include <atomic>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <thread>
#include <type_traits>
#include <variant>

#if defined(DEVICE_NVVM) || defined(DEVICE_AMD)
#define DEVICE_GPU
#endif

static inline size_t roundUp(size_t num, size_t multiple)
{
    if (multiple == 0)
        return num;

    int remainder = num % multiple;
    if (remainder == 0)
        return num;

    return num + multiple - remainder;
}

enum class BufferRequestFlags {
    Clear       = 0x1, // Will clear the buffer (setting it bytewise to 0) on each call to requestBuffer
    ClearOnInit = 0x2  // Will clear the buffer (setting it bytewise to 0) only the first time this specific buffer was requested
};

template <typename T>
void clearArray(anydsl::Array<T>& array)
{
    if (array.size() == 0)
        return;

    if (array.device() == 0) { // CPU
        std::memset(array.data(), 0, array.size() * sizeof(T));
    } else {
        // FIXME: This is bad behaviour. We need a memset function in the anydsl_runtime
        // or add more utility functions to artic
        thread_local std::vector<uint8_t> clear_buffer;

        size_t dst_size = array.size() * sizeof(T);
        if (clear_buffer.size() < dst_size) {
            size_t off = clear_buffer.size();
            size_t len = dst_size - clear_buffer.size();
            clear_buffer.resize(dst_size);
            std::memset(clear_buffer.data() + off, 0, len);
        }

        anydsl_copy(0 /*Host*/, (void*)clear_buffer.data(), 0,
                    array.device(), (void*)array.data(), 0,
                    dst_size);
    }
}

// TODO: It would be great to get the number below automatically
constexpr size_t MaxRayPayloadComponents = 8;
constexpr size_t RayStreamSize           = 9;
constexpr size_t PrimaryStreamSize       = RayStreamSize + 6 + MaxRayPayloadComponents;
constexpr size_t SecondaryStreamSize     = RayStreamSize + 4;

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
    size_t EntryCount;
    ShallowArray<LookupEntry> LookupEntries;
    ShallowArray<uint8_t> Data;
};

struct SceneDatabaseProxy {
    DynTableProxy Entities;
    DynTableProxy Shapes;
    DynTableProxy BVHs;
};

constexpr size_t GPUStreamBufferCount = 2;
struct Interface {
    using DeviceImage  = std::tuple<anydsl::Array<float>, int32_t, int32_t>;
    using DeviceBuffer = std::tuple<anydsl::Array<uint8_t>, int32_t>;

    struct DeviceData {
        std::atomic_flag scene_loaded;
        BvhVariant bvh_ent;
        std::atomic_flag database_loaded;
        SceneDatabaseProxy database;
        anydsl::Array<int32_t> tmp_buffer;
        anydsl::Array<int32_t> ray_begins_buffer; // On the host
        anydsl::Array<int32_t> ray_ends_buffer;   // On the host
        std::array<anydsl::Array<float>, GPUStreamBufferCount> primary;
        std::array<anydsl::Array<float>, GPUStreamBufferCount> secondary;
        std::vector<anydsl::Array<float>> aovs;
        anydsl::Array<float> film_pixels;
        anydsl::Array<StreamRay> ray_list;
        std::array<anydsl::Array<float>*, GPUStreamBufferCount> current_primary;
        std::array<anydsl::Array<float>*, GPUStreamBufferCount> current_secondary;
        std::unordered_map<std::string, DeviceImage> images;
        std::unordered_map<std::string, DeviceBuffer> buffers;

        anydsl::Array<uint32_t> tonemap_pixels;

        inline DeviceData()
            : scene_loaded(ATOMIC_FLAG_INIT)
            , database_loaded(ATOMIC_FLAG_INIT)
        {
            for (size_t i = 0; i < primary.size(); ++i)
                current_primary[i] = &primary[i];
            for (size_t i = 0; i < secondary.size(); ++i)
                current_secondary[i] = &secondary[i];
        }

        ~DeviceData() = default;
    };
    std::unordered_map<int32_t, DeviceData> devices;

    struct CPUData {
        anydsl::Array<float> cpu_primary;
        anydsl::Array<float> cpu_secondary;
        IG::Statistics stats;
    };
    std::mutex thread_mutex;
    std::unordered_map<std::thread::id, std::unique_ptr<CPUData>> thread_data;

    std::vector<anydsl::Array<float>> aovs;
    anydsl::Array<float> host_pixels;
    const IG::SceneDatabase* database;
    size_t film_width;
    size_t film_height;

    IG::uint32 current_iteration;
    Settings current_settings;

    const IG::Ray* ray_list = nullptr; // film_width contains number of rays

    DriverSetupSettings setup;
    IG::TechniqueVariantShaderSet shader_set;

    IG::Statistics main_stats;

    inline Interface(const DriverSetupSettings& setup)
        : aovs(setup.aov_count)
        , host_pixels(setup.framebuffer_width * setup.framebuffer_height * 3)
        , database(setup.database)
        , film_width(setup.framebuffer_width)
        , film_height(setup.framebuffer_height)
        , setup(setup)
    {
        // Due to the DLL interface, we do have multiple instances of the logger. Make sure they are the same
        IG_LOGGER = *setup.logger;

        for (auto& arr : aovs)
            arr = std::move(anydsl::Array<float>(film_width * film_height * 3));
    }

    inline ~Interface()
    {
    }

    inline void setShaderSet(const IG::TechniqueVariantShaderSet& shader_set)
    {
        this->shader_set = shader_set;
    }

    template <typename T>
    inline anydsl::Array<T>& resizeArray(int32_t dev, anydsl::Array<T>& array, size_t size, size_t multiplier)
    {
        const auto capacity = (size & ~((1 << 5) - 1)) + 32; // round to 32
        if (array.size() < (int64_t)capacity) {
            auto n = capacity * multiplier;
            array  = std::move(anydsl::Array<T>(dev, reinterpret_cast<T*>(anydsl_alloc(dev, sizeof(T) * n)), n));
        }
        return array;
    }

    inline CPUData* getThreadData()
    {
        // FIXME: This prevents the driver to be used multiple times over the lifetime of the application
        // It works fine if the settings are the same and/or the exact same threads are not used again
        thread_local CPUData* dataptr = nullptr;
        if (!dataptr) {
            thread_mutex.lock();
            if (!thread_data.count(std::this_thread::get_id()))
                thread_data[std::this_thread::get_id()] = std::make_unique<CPUData>();

            dataptr = thread_data[std::this_thread::get_id()].get();
            thread_mutex.unlock();
        }
        return dataptr;
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
        return resizeArray(dev, *devices[dev].current_primary[buffer], size, PrimaryStreamSize);
    }

    inline anydsl::Array<float>& getGPUPrimaryStream(int32_t dev, size_t buffer)
    {
        IG_ASSERT(devices[dev].current_primary[buffer]->size() > 0, "Expected gpu primary stream to be initialized");
        return *devices[dev].current_primary[buffer];
    }

    inline anydsl::Array<float>& getGPUSecondaryStream(int32_t dev, size_t buffer, size_t size)
    {
        return resizeArray(dev, *devices[dev].current_secondary[buffer], size, SecondaryStreamSize);
    }

    inline anydsl::Array<float>& getGPUSecondaryStream(int32_t dev, size_t buffer)
    {
        IG_ASSERT(devices[dev].current_secondary[buffer]->size() > 0, "Expected gpu secondary stream to be initialized");
        return *devices[dev].current_secondary[buffer];
    }

    inline size_t getGPUTemporaryBufferSize() const
    {
        // Upper bound extracted from "mapping_gpu.art"
        return std::max<size_t>(32, database->EntityTable.entryCount() + 1);
    }

    inline anydsl::Array<int32_t>& getGPUTemporaryBuffer(int32_t dev)
    {
        return resizeArray(dev, devices[dev].tmp_buffer, getGPUTemporaryBufferSize(), 1);
    }

    inline auto getGPURayBeginEndBuffers(int32_t dev)
    {
        // Even while the ray begins & ends are on the CPU only the GPU backend makes use of it
        const size_t size = getGPUTemporaryBufferSize();
        return std::forward_as_tuple(
            resizeArray(0 /*Host*/, devices[dev].ray_begins_buffer, size, 1),
            resizeArray(0 /*Host*/, devices[dev].ray_ends_buffer, size, 1));
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

        std::vector<StreamRay> rays;
        rays.reserve(film_width);

        for (size_t i = 0; i < film_width; ++i) {
            const IG::Ray dRay = ray_list[i];

            float norm = dRay.Direction.norm();
            if (norm < std::numeric_limits<float>::epsilon()) {
                std::cerr << "Invalid ray given: Ray has zero direction!" << std::endl;
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

        return device.ray_list = std::move(copyToDevice(dev, rays));
    }

    template <typename T>
    inline anydsl::Array<T> copyToDevice(int32_t dev, const T* data, size_t n)
    {
        if (n == 0)
            return anydsl::Array<T>();

        anydsl::Array<T> array(dev, reinterpret_cast<T*>(anydsl_alloc(dev, n * sizeof(T))), n);
        anydsl_copy(0, data, 0, dev, array.data(), 0, sizeof(T) * n);
        return array;
    }

    template <typename T>
    inline anydsl::Array<T> copyToDevice(int32_t dev, const std::vector<T>& host)
    {
        return copyToDevice(dev, host.data(), host.size());
    }

    inline DeviceImage copyToDevice(int32_t dev, const IG::ImageRgba32& image)
    {
        return DeviceImage(copyToDevice(dev, image.pixels.get(), image.width * image.height * 4), image.width, image.height);
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
        proxy.LookupEntries = std::move(ShallowArray<LookupEntry>(dev, (LookupEntry*)tbl.lookups().data(), tbl.lookups().size()));
        proxy.Data          = std::move(ShallowArray<uint8_t>(dev, tbl.data().data(), tbl.data().size()));
        return proxy;
    }

    // Load all the data assembled in previous stages to the device
    inline const SceneDatabaseProxy& loadSceneDatabase(int32_t dev)
    {
        auto& device = devices[dev];
        if (device.database_loaded.test_and_set())
            return device.database;

        SceneDatabaseProxy& proxy = device.database;

        proxy.Entities = std::move(loadDyntable(dev, database->EntityTable));
        proxy.Shapes   = std::move(loadDyntable(dev, database->ShapeTable));
        proxy.BVHs     = std::move(loadDyntable(dev, database->BVHTable));

        return proxy;
    }

    inline SceneInfo loadSceneInfo(int32_t dev)
    {
        IG_UNUSED(dev);

        SceneInfo info;
        info.num_entities = database->EntityTable.entryCount();
        return info;
    }

    inline const DeviceImage& loadImage(int32_t dev, const std::string& filename)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& images = devices[dev].images;
        auto it      = images.find(filename);
        if (it != images.end())
            return it->second;

        IG_LOG(IG::L_DEBUG) << "Loading image " << filename << std::endl;
        try {
            return images[filename] = std::move(copyToDevice(dev, IG::ImageRgba32::load(filename)));
        } catch (const IG::ImageLoadException& e) {
            IG_LOG(IG::L_ERROR) << e.what() << std::endl;
            return images[filename] = std::move(copyToDevice(dev, IG::ImageRgba32()));
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

        IG_LOG(IG::L_DEBUG) << "Loading buffer " << filename << std::endl;
        const auto vec = readBufferFile(filename);

        if ((vec.size() % sizeof(int32_t)) != 0)
            IG_LOG(IG::L_WARNING) << "Buffer " << filename << " is not properly sized!" << std::endl;

        return buffers[filename] = std::move(DeviceBuffer(
                   std::move(copyToDevice(dev, vec)), vec.size()));
    }

    inline DeviceBuffer& requestBuffer(int32_t dev, const std::string& name, int32_t size, int32_t flags)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        IG_ASSERT(size > 0, "Expected buffer size to be larger then zero");

        // Make sure the buffer is properly sized
        size = roundUp(size, 32);

        auto& buffers = devices[dev].buffers;
        auto it       = buffers.find(name);
        if (it != buffers.end() && std::get<1>(it->second) >= size) {
            if (flags & (int)BufferRequestFlags::Clear)
                clearArray(std::get<0>(it->second));
            return it->second;
        }

        IG_LOG(IG::L_DEBUG) << "Requested buffer " << name << " with " << size << " bytes" << std::endl;
        buffers[name] = std::move(DeviceBuffer(
            std::move(anydsl::Array<uint8_t>(dev, reinterpret_cast<uint8_t*>(anydsl_alloc(dev, size)), size)),
            size));

        if ((flags & (int)BufferRequestFlags::ClearOnInit) || (flags & (int)BufferRequestFlags::Clear))
            clearArray(std::get<0>(buffers[name]));

        return buffers[name];
    }

    inline int runRayGenerationShader(int* id, int size, int xmin, int ymin, int xmax, int ymax)
    {
        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(IG::ShaderType::RayGeneration, {});

        using Callback = decltype(ig_ray_generation_shader);
        IG_ASSERT(shader_set.RayGenerationShader != nullptr, "Expected ray generation shader to be valid");
        auto callback = (Callback*)shader_set.RayGenerationShader;
        const int ret = callback(&current_settings, current_iteration, id, size, xmin, ymin, xmax, ymax);

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(IG::ShaderType::RayGeneration, {});
        return ret;
    }

    inline void runMissShader(int first, int last)
    {
        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(IG::ShaderType::Miss, {});

        using Callback = decltype(ig_miss_shader);
        IG_ASSERT(shader_set.MissShader != nullptr, "Expected miss shader to be valid");
        auto callback = (Callback*)shader_set.MissShader;
        callback(&current_settings, first, last);

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(IG::ShaderType::Miss, {});
    }

    inline void runHitShader(int entity_id, int first, int last)
    {
        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(IG::ShaderType::Hit, entity_id);

        using Callback = decltype(ig_hit_shader);
        IG_ASSERT(entity_id >= 0 && entity_id < (int)shader_set.HitShaders.size(), "Expected entity id for hit shaders to be valid");
        void* hit_shader = shader_set.HitShaders[entity_id];
        IG_ASSERT(hit_shader != nullptr, "Expected hit shader to be valid");
        auto callback = (Callback*)hit_shader;
        callback(&current_settings, entity_id, first, last);

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(IG::ShaderType::Hit, entity_id);
    }

    inline bool useAdvancedShadowHandling()
    {
        return shader_set.AdvancedShadowHitShader != nullptr && shader_set.AdvancedShadowMissShader != nullptr;
    }

    inline bool isFramebufferLocked()
    {
        return shader_set.LockFramebuffer;
    }

    inline void runAdvancedShadowShader(int first, int last, bool is_hit)
    {
        IG_ASSERT(useAdvancedShadowHandling(), "Expected advanced shadow shader only be called if it is enabled!");

        if (is_hit) {
            if (setup.acquire_stats)
                getThreadData()->stats.beginShaderLaunch(IG::ShaderType::AdvancedShadowHit, {});

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(shader_set.AdvancedShadowHitShader != nullptr, "Expected miss shader to be valid");
            auto callback = (Callback*)shader_set.AdvancedShadowHitShader;
            callback(&current_settings, first, last);

            if (setup.acquire_stats)
                getThreadData()->stats.endShaderLaunch(IG::ShaderType::AdvancedShadowHit, {});
        } else {
            if (setup.acquire_stats)
                getThreadData()->stats.beginShaderLaunch(IG::ShaderType::AdvancedShadowMiss, {});

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(shader_set.AdvancedShadowMissShader != nullptr, "Expected miss shader to be valid");
            auto callback = (Callback*)shader_set.AdvancedShadowMissShader;
            callback(&current_settings, first, last);

            if (setup.acquire_stats)
                getThreadData()->stats.endShaderLaunch(IG::ShaderType::AdvancedShadowMiss, {});
        }
    }

    inline void runCallbackShader(int type)
    {
        IG_ASSERT(type >= 0 && type < (int)IG::CallbackType::_COUNT, "Expected callback shader type to be well formed!");

        if (shader_set.CallbackShaders[type] != nullptr) {
            using Callback = decltype(ig_callback_shader);
            auto callback  = (Callback*)shader_set.CallbackShaders[type];
            callback(&current_settings, current_iteration);
        }
    }

    inline float* getFilmImage(int32_t dev)
    {
        if (dev != 0) {
            auto& device = devices[dev];
            if (!device.film_pixels.size()) {
                auto film_size     = film_width * film_height * 3;
                auto film_data     = reinterpret_cast<float*>(anydsl_alloc(dev, sizeof(float) * film_size));
                device.film_pixels = std::move(anydsl::Array<float>(dev, film_data, film_size));
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
        IG_ASSERT(index < aovs.size(), "AOV index out of bounds!");

        if (dev != 0) {
            auto& device = devices[dev];
            if (device.aovs.size() != aovs.size())
                device.aovs.resize(aovs.size());

            if (!device.aovs[index].size()) {
                auto film_size     = film_width * film_height * 3;
                auto film_data     = reinterpret_cast<float*>(anydsl_alloc(dev, sizeof(float) * film_size));
                device.aovs[index] = std::move(anydsl::Array<float>(dev, film_data, film_size));
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
        auto& device = devices[dev];
        if (!device.tonemap_pixels.size()) {
            auto film_size        = film_width * film_height;
            auto film_data        = reinterpret_cast<uint32_t*>(anydsl_alloc(dev, sizeof(uint32_t) * film_size));
            device.tonemap_pixels = std::move(anydsl::Array<uint32_t>(dev, film_data, film_size));
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
                if (device_pixels.size())
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
                    if (device_pixels.size())
                        anydsl::copy(buffer, device_pixels);
                }
            }
        }
    }

    inline IG::Statistics* getFullStats()
    {
        main_stats.reset();
        for (const auto& pair : thread_data)
            main_stats.add(pair.second->stats);

        return &main_stats;
    }
};

static std::unique_ptr<Interface> sInterface;

static Settings convert_settings(const DriverRenderSettings* settings, IG::uint32 iter)
{
    Settings renderSettings;
    renderSettings.device  = settings->device;
    renderSettings.spi     = settings->spi;
    renderSettings.width   = settings->width;
    renderSettings.height  = settings->height;
    renderSettings.eye.x   = settings->eye[0];
    renderSettings.eye.y   = settings->eye[1];
    renderSettings.eye.z   = settings->eye[2];
    renderSettings.dir.x   = settings->dir[0];
    renderSettings.dir.y   = settings->dir[1];
    renderSettings.dir.z   = settings->dir[2];
    renderSettings.up.x    = settings->up[0];
    renderSettings.up.y    = settings->up[1];
    renderSettings.up.z    = settings->up[2];
    renderSettings.right.x = settings->right[0];
    renderSettings.right.y = settings->right[1];
    renderSettings.right.z = settings->right[2];
    renderSettings.tmin    = settings->tmin;
    renderSettings.tmax    = settings->tmax;
    renderSettings.iter    = (int)iter;

    renderSettings.debug_mode = settings->debug_mode;

    return renderSettings;
}

void glue_render(const DriverRenderSettings* settings, IG::uint32 iter)
{
    Settings renderSettings = convert_settings(settings, iter);

    sInterface->ray_list          = settings->rays;
    sInterface->current_iteration = iter;
    sInterface->current_settings  = renderSettings;

    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.beginShaderLaunch(IG::ShaderType::Device, {});

    ig_render(&renderSettings);

    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.endShaderLaunch(IG::ShaderType::Device, {});
}

void glue_setup(const DriverSetupSettings* settings)
{
    sInterface = std::make_unique<Interface>(*settings);
}

void glue_setShaderSet(const IG::TechniqueVariantShaderSet& shaderSet)
{
    sInterface->setShaderSet(shaderSet);
}

void glue_shutdown()
{
    sInterface.reset();
}

const float* glue_getFramebuffer(int aov)
{
    if (aov <= 0 || (size_t)aov > sInterface->aovs.size())
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

void glue_tonemap(int device, int aov, uint32_t* out_pixels, int flags, float scale, float exposure_factor, float exposure_offset)
{
    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.beginShaderLaunch(IG::ShaderType::Tonemap, {});

#ifdef DEVICE_GPU
#if defined(DEVICE_NVVM)
    int dev_id = ANYDSL_DEVICE(ANYDSL_CUDA, device);
#elif defined(DEVICE_AMD)
    int dev_id = ANYDSL_DEVICE(ANYDSL_HSA, device);
#endif
    float* in_pixels            = sInterface->getAOVImage(dev_id, aov);
    uint32_t* device_out_pixels = sInterface->getTonemapImage(dev_id);
#else
    float* in_pixels            = sInterface->getAOVImage(0, aov);
    uint32_t* device_out_pixels = out_pixels;
#endif

    ig_tonemap(device, in_pixels, device_out_pixels, sInterface->film_width, sInterface->film_height, flags, scale, exposure_factor, exposure_offset);

#ifdef DEVICE_GPU
    int size = sInterface->film_width * sInterface->film_height;
    anydsl_copy(dev_id, device_out_pixels, 0, 0 /* Host */, out_pixels, 0, sizeof(uint32_t) * size);
#endif

    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.endShaderLaunch(IG::ShaderType::Tonemap, {});
}

inline void get_ray_stream(RayStream& rays, float* ptr, size_t capacity)
{
    static_assert(std::is_pod<RayStream>::value, "Expected RayStream to be plain old data");

    float** r_ptr = reinterpret_cast<float**>(&rays);
    for (size_t i = 0; i < RayStreamSize; ++i)
        r_ptr[i] = ptr + i * capacity;
}

inline void get_primary_stream(PrimaryStream& primary, float* ptr, size_t capacity, size_t components)
{
    static_assert(std::is_pod<PrimaryStream>::value, "Expected PrimaryStream to be plain old data");

    get_ray_stream(primary.rays, ptr, capacity);

    float** r_ptr = reinterpret_cast<float**>(&primary);
    for (size_t i = RayStreamSize; i < components; ++i)
        r_ptr[i] = ptr + i * capacity;

    primary.size = 0;
}

inline void get_secondary_stream(SecondaryStream& secondary, float* ptr, size_t capacity)
{
    static_assert(std::is_pod<SecondaryStream>::value, "Expected SecondaryStream to be plain old data");

    get_ray_stream(secondary.rays, ptr, capacity);

    float** r_ptr = reinterpret_cast<float**>(&secondary);
    for (size_t i = RayStreamSize; i < SecondaryStreamSize; ++i)
        r_ptr[i] = ptr + i * capacity;

    secondary.size = 0;
}

extern "C" {
IG_EXPORT DriverInterface ig_get_interface()
{
    DriverInterface interface;
    interface.MajorVersion = IG_VERSION_MAJOR;
    interface.MinorVersion = IG_VERSION_MINOR;

    interface.Target = IG::Target::INVALID;
// Expose Target
#if defined(DEVICE_DEFAULT)
    interface.Target = IG::Target::GENERIC;
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

    interface.SetupFunction            = glue_setup;
    interface.ShutdownFunction         = glue_shutdown;
    interface.RenderFunction           = glue_render;
    interface.SetShaderSetFunction     = glue_setShaderSet;
    interface.GetFramebufferFunction   = glue_getFramebuffer;
    interface.ClearFramebufferFunction = glue_clearFramebuffer;
    interface.GetStatisticsFunction    = glue_getStatistics;
    interface.TonemapFunction          = glue_tonemap;

    return interface;
}

void ignis_get_film_data(int dev, float** pixels, int* width, int* height)
{
    *pixels = sInterface->getFilmImage(dev);
    *width  = sInterface->film_width;
    *height = sInterface->film_height;
}

void ignis_get_aov_image(int dev, int id, float** aov_pixels)
{
    *aov_pixels = sInterface->getAOVImage(dev, id);
}

void ignis_get_work_info(int* width, int* height)
{
    if (sInterface->shader_set.Width > 0 && sInterface->shader_set.Height > 0) {
        *width  = sInterface->shader_set.Width;
        *height = sInterface->shader_set.Height;
    } else {
        *width  = sInterface->film_width;
        *height = sInterface->film_height;
    }
}

void ignis_load_bvh2_ent(int dev, Node2** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<Bvh2Ent, Node2>(dev);
    *nodes    = const_cast<Node2*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

void ignis_load_bvh4_ent(int dev, Node4** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<Bvh4Ent, Node4>(dev);
    *nodes    = const_cast<Node4*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

void ignis_load_bvh8_ent(int dev, Node8** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->loadEntityBVH<Bvh8Ent, Node8>(dev);
    *nodes    = const_cast<Node8*>(bvh.Nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.Objs.ptr());
}

void ignis_load_scene(int dev, SceneDatabase* dtb)
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

void ignis_load_scene_info(int dev, SceneInfo* info)
{
    *info = sInterface->loadSceneInfo(dev);
}

void ignis_load_rays(int dev, StreamRay** list)
{
    *list = const_cast<StreamRay*>(sInterface->loadRayList(dev).data());
}

void ignis_load_image(int32_t dev, const char* file, float** pixels, int32_t* width, int32_t* height)
{
    auto& img = sInterface->loadImage(dev, file);
    *pixels   = const_cast<float*>(std::get<0>(img).data());
    *width    = std::get<1>(img);
    *height   = std::get<2>(img);
}

void ignis_load_buffer(int32_t dev, const char* file, uint8_t** data, int32_t* size)
{
    auto& img = sInterface->loadBuffer(dev, file);
    *data     = const_cast<uint8_t*>(std::get<0>(img).data());
    *size     = std::get<1>(img);
}

void ignis_request_buffer(int32_t dev, const char* name, uint8_t** data, int size, int flags)
{
    auto& buffer = sInterface->requestBuffer(dev, name, size, flags);
    *data        = const_cast<uint8_t*>(std::get<0>(buffer).data());
}

void ignis_cpu_get_primary_stream(PrimaryStream* primary, int size)
{
    auto& array = sInterface->getCPUPrimaryStream(size);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_cpu_get_primary_stream_const(PrimaryStream* primary)
{
    auto& array = sInterface->getCPUPrimaryStream();
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_cpu_get_secondary_stream(SecondaryStream* secondary, int size)
{
    auto& array = sInterface->getCPUSecondaryStream(size);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_cpu_get_secondary_stream_const(SecondaryStream* secondary)
{
    auto& array = sInterface->getCPUSecondaryStream();
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_gpu_get_tmp_buffer(int dev, int** buf)
{
    *buf = sInterface->getGPUTemporaryBuffer(dev).data();
}

void ignis_gpu_get_ray_begin_end_buffers(int dev, int** ray_begins, int** ray_ends)
{
    auto tuple  = sInterface->getGPURayBeginEndBuffers(dev);
    *ray_begins = std::get<0>(tuple).data();
    *ray_ends   = std::get<1>(tuple).data();
}

void ignis_gpu_get_first_primary_stream(int dev, PrimaryStream* primary, int size)
{
    auto& array = sInterface->getGPUPrimaryStream(dev, 0, size);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_gpu_get_first_primary_stream_const(int dev, PrimaryStream* primary)
{
    auto& array = sInterface->getGPUPrimaryStream(dev, 0);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_gpu_get_second_primary_stream(int dev, PrimaryStream* primary, int size)
{
    auto& array = sInterface->getGPUPrimaryStream(dev, 1, size);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_gpu_get_second_primary_stream_const(int dev, PrimaryStream* primary)
{
    auto& array = sInterface->getGPUPrimaryStream(dev, 1);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_gpu_get_first_secondary_stream(int dev, SecondaryStream* secondary, int size)
{
    auto& array = sInterface->getGPUSecondaryStream(dev, 0, size);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_gpu_get_first_secondary_stream_const(int dev, SecondaryStream* secondary)
{
    auto& array = sInterface->getGPUSecondaryStream(dev, 0);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_gpu_get_second_secondary_stream(int dev, SecondaryStream* secondary, int size)
{
    auto& array = sInterface->getGPUSecondaryStream(dev, 1, size);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_gpu_get_second_secondary_stream_const(int dev, SecondaryStream* secondary)
{
    auto& array = sInterface->getGPUSecondaryStream(dev, 1);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_gpu_swap_primary_streams(int dev)
{
    sInterface->swapGPUPrimaryStreams(dev);
}

void ignis_gpu_swap_secondary_streams(int dev)
{
    sInterface->swapGPUSecondaryStreams(dev);
}

int ignis_handle_ray_generation(int* id, int size, int xmin, int ymin, int xmax, int ymax)
{
    return sInterface->runRayGenerationShader(id, size, xmin, ymin, xmax, ymax);
}

void ignis_handle_miss_shader(int first, int last)
{
    sInterface->runMissShader(first, last);
}

void ignis_handle_hit_shader(int entity_id, int first, int last)
{
    sInterface->runHitShader(entity_id, first, last);
}

void ignis_handle_advanced_shadow_shader(int first, int last, bool is_hit)
{
    sInterface->runAdvancedShadowShader(first, last, is_hit);
}

void ignis_handle_callback_shader(int type)
{
    sInterface->runCallbackShader(type);
}

bool ignis_use_advanced_shadow_handling()
{
    return sInterface->useAdvancedShadowHandling();
}

bool ignis_is_framebuffer_locked()
{
    return sInterface->isFramebufferLocked();
}

void ignis_present(int dev)
{
    if (dev != 0)
        sInterface->present(dev);
}

void ig_host_print(const char* str)
{
    std::cout << str << std::flush;
}

void ig_host_print_i(int64_t number)
{
    std::cout << number << std::flush;
}

void ig_host_print_f(double number)
{
    std::cout << number << std::flush;
}

void ig_host_print_p(const char* ptr)
{
    std::cout << std::internal << std::hex << std::setw(sizeof(ptr) * 2 + 2) << std::setfill('0')
              << (const void*)ptr
              << std::resetiosflags(std::ios::internal) << std::resetiosflags(std::ios::hex)
              << std::flush;
}

int64_t clock_us()
{
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#define CPU_FREQ 4e9
    __asm__ __volatile__("xorl %%eax,%%eax \n    cpuid" ::
                             : "%rax", "%rbx", "%rcx", "%rdx");
    return __rdtsc() * int64_t(1000000) / int64_t(CPU_FREQ);
#else
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
#endif
}
}
