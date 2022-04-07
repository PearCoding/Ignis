#include "Image.h"
#include "Logger.h"
#include "RuntimeStructs.h"
#include "Statistics.h"
#include "config/Version.h"
#include "driver/Interface.h"
#include "table/SceneDatabase.h"

#include "generated_interface.h"

#include "ShallowArray.h"

#include <anydsl_jit.h>
#include <anydsl_runtime.hpp>

#include <atomic>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <thread>
#include <type_traits>
#include <variant>

#include <tbb/concurrent_vector.h>

#if defined(DEVICE_NVVM) || defined(DEVICE_AMD)
#define DEVICE_GPU
#endif

static inline size_t roundUp(size_t num, size_t multiple)
{
    if (multiple == 0)
        return num;

    size_t remainder = num % multiple;
    if (remainder == 0)
        return num;

    return num + multiple - remainder;
}

static Settings convert_settings(const DriverRenderSettings& settings, size_t iter)
{
    Settings renderSettings;
    renderSettings.device = (int)settings.device;
    renderSettings.spi    = (int)settings.spi;
    renderSettings.iter   = (int)iter;
    renderSettings.width  = (int)settings.work_width;
    renderSettings.height = (int)settings.work_height;

    return renderSettings;
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
    size_t EntryCount = 0;
    ShallowArray<LookupEntry> LookupEntries;
    ShallowArray<uint8_t> Data;
};

struct SceneDatabaseProxy {
    DynTableProxy Entities;
    DynTableProxy Shapes;
    DynTableProxy BVHs;
};

constexpr size_t InvalidThreadID = (size_t)-1;
thread_local size_t sThreadID    = InvalidThreadID;

constexpr size_t GPUStreamBufferCount = 2;
class Interface {
    IG_CLASS_NON_COPYABLE(Interface);
    IG_CLASS_NON_MOVEABLE(Interface);

public:
    using DeviceImage  = std::tuple<anydsl::Array<float>, size_t, size_t>;
    using DeviceBuffer = std::tuple<anydsl::Array<uint8_t>, size_t>;

    class DeviceData {
        IG_CLASS_NON_COPYABLE(DeviceData);
        IG_CLASS_NON_MOVEABLE(DeviceData);

    public:
        std::atomic_flag scene_loaded = ATOMIC_FLAG_INIT;
        BvhVariant bvh_ent;
        std::atomic_flag database_loaded = ATOMIC_FLAG_INIT;
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

    struct CPUData {
        anydsl::Array<float> cpu_primary;
        anydsl::Array<float> cpu_secondary;
        IG::Statistics stats;
    };
    std::mutex thread_mutex;
    tbb::concurrent_vector<std::unique_ptr<CPUData>> thread_data;

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
    }

    inline ~Interface() = default;

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
            auto n = capacity * multiplier;
            array  = std::move(anydsl::Array<T>(dev, reinterpret_cast<T*>(anydsl_alloc(dev, sizeof(T) * n)), n));
        }
        return array;
    }

    inline void registerThread()
    {
        if (sThreadID != InvalidThreadID)
            return;

        // FIXME: This does not work well on Windows. New threads are generated continously...
        // This polutes the array which will get out of memory for extreme work loads
        const auto it = thread_data.emplace_back(std::make_unique<CPUData>());
        sThreadID     = std::distance(thread_data.begin(), it);

        IG_LOG(IG::L_DEBUG) << "Registering thread 0x" << std::hex << std::this_thread::get_id() << " with id 0x" << std::hex << sThreadID << std::endl;
    }

    inline CPUData* getThreadData()
    {
        IG_ASSERT(sThreadID != InvalidThreadID, "Thread not registered");
        return thread_data.at(sThreadID).get();
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

        IG_ASSERT(current_settings.rays != nullptr, "Expected list of rays to be available");

        std::vector<StreamRay> rays;
        rays.reserve(film_width);

        for (size_t i = 0; i < film_width; ++i) {
            const auto dRay = current_settings.rays[i];

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

        return device.ray_list = copyToDevice(dev, rays);
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

    inline const DeviceImage& loadImage(int32_t dev, const std::string& filename)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& images = devices[dev].images;
        auto it      = images.find(filename);
        if (it != images.end())
            return it->second;

        IG_LOG(IG::L_DEBUG) << "Loading image " << filename << std::endl;
        try {
            return images[filename] = copyToDevice(dev, IG::ImageRgba32::load(filename));
        } catch (const IG::ImageLoadException& e) {
            IG_LOG(IG::L_ERROR) << e.what() << std::endl;
            return images[filename] = copyToDevice(dev, IG::ImageRgba32());
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

        IG_LOG(IG::L_DEBUG) << "Requested buffer " << name << " with " << size << " bytes" << std::endl;
        buffers[name] = DeviceBuffer(anydsl::Array<uint8_t>(dev, reinterpret_cast<uint8_t*>(anydsl_alloc(dev, size)), size), size);

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

    inline int runRayGenerationShader(int* id, int size, int xmin, int ymin, int xmax, int ymax)
    {
        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(IG::ShaderType::RayGeneration, {});

        using Callback = decltype(ig_ray_generation_shader);
        IG_ASSERT(shader_set.RayGenerationShader != nullptr, "Expected ray generation shader to be valid");
        auto callback = reinterpret_cast<Callback*>(shader_set.RayGenerationShader);
        const int ret = callback(&driver_settings, (int)current_iteration, id, size, xmin, ymin, xmax, ymax);

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
        auto callback = reinterpret_cast<Callback*>(shader_set.MissShader);
        callback(&driver_settings, first, last);

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(IG::ShaderType::Miss, {});
    }

    inline void runHitShader(int entity_id, int first, int last)
    {
        const int material_id = database->EntityToMaterial.at(entity_id);

        if (setup.acquire_stats)
            getThreadData()->stats.beginShaderLaunch(IG::ShaderType::Hit, material_id);

        using Callback = decltype(ig_hit_shader);
        IG_ASSERT(material_id >= 0 && material_id < (int)shader_set.HitShaders.size(), "Expected material id for hit shaders to be valid");
        void* hit_shader = shader_set.HitShaders.at(material_id);
        IG_ASSERT(hit_shader != nullptr, "Expected hit shader to be valid");
        auto callback = reinterpret_cast<Callback*>(hit_shader);
        callback(&driver_settings, entity_id, first, last);

        if (setup.acquire_stats)
            getThreadData()->stats.endShaderLaunch(IG::ShaderType::Hit, material_id);
    }

    inline bool useAdvancedShadowHandling()
    {
        return shader_set.AdvancedShadowHitShader != nullptr && shader_set.AdvancedShadowMissShader != nullptr;
    }

    inline bool isFramebufferLocked()
    {
        return current_settings.framebuffer_locked;
    }

    inline void runAdvancedShadowShader(int first, int last, bool is_hit)
    {
        IG_ASSERT(useAdvancedShadowHandling(), "Expected advanced shadow shader only be called if it is enabled!");

        if (is_hit) {
            if (setup.acquire_stats)
                getThreadData()->stats.beginShaderLaunch(IG::ShaderType::AdvancedShadowHit, {});

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(shader_set.AdvancedShadowHitShader != nullptr, "Expected miss shader to be valid");
            auto callback = reinterpret_cast<Callback*>(shader_set.AdvancedShadowHitShader);
            callback(&driver_settings, first, last);

            if (setup.acquire_stats)
                getThreadData()->stats.endShaderLaunch(IG::ShaderType::AdvancedShadowHit, {});
        } else {
            if (setup.acquire_stats)
                getThreadData()->stats.beginShaderLaunch(IG::ShaderType::AdvancedShadowMiss, {});

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(shader_set.AdvancedShadowMissShader != nullptr, "Expected miss shader to be valid");
            auto callback = reinterpret_cast<Callback*>(shader_set.AdvancedShadowMissShader);
            callback(&driver_settings, first, last);

            if (setup.acquire_stats)
                getThreadData()->stats.endShaderLaunch(IG::ShaderType::AdvancedShadowMiss, {});
        }
    }

    inline void runCallbackShader(int type)
    {
        IG_ASSERT(type >= 0 && type < (int)IG::CallbackType::_COUNT, "Expected callback shader type to be well formed!");

        if (shader_set.CallbackShaders[type] != nullptr) {
            using Callback = decltype(ig_callback_shader);
            auto callback  = reinterpret_cast<Callback*>(shader_set.CallbackShaders[type]);
            callback(&driver_settings, (int)current_iteration);
        }
    }

    inline float* getFilmImage(int32_t dev)
    {
        if (dev != 0) {
            auto& device = devices[dev];
            if (device.film_pixels.size() != host_pixels.size()) {
                auto film_size     = film_width * film_height * 3;
                auto film_data     = reinterpret_cast<float*>(anydsl_alloc(dev, sizeof(float) * film_size));
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
                auto film_size     = film_width * film_height * 3;
                auto film_data     = reinterpret_cast<float*>(anydsl_alloc(dev, sizeof(float) * film_size));
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
        auto& device = devices[dev];
        if (device.tonemap_pixels.size() != host_pixels.size()) {
            auto film_size        = film_width * film_height;
            auto film_data        = reinterpret_cast<uint32_t*>(anydsl_alloc(dev, sizeof(uint32_t) * film_size));
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
    int getParameterInt(const char* name, int def)
    {
        IG_ASSERT(current_parameters != nullptr, "No parameters available!");
        if (current_parameters->IntParameters.count(name) > 0)
            return current_parameters->IntParameters.at(name);
        else
            return def;
    }

    float getParameterFloat(const char* name, float def)
    {
        IG_ASSERT(current_parameters != nullptr, "No parameters available!");
        if (current_parameters->FloatParameters.count(name) > 0)
            return current_parameters->FloatParameters.at(name);
        else
            return def;
    }

    void getParameterVector(const char* name, float defX, float defY, float defZ, float& outX, float& outY, float& outZ)
    {
        IG_ASSERT(current_parameters != nullptr, "No parameters available!");
        if (current_parameters->VectorParameters.count(name) > 0) {
            IG::Vector3f param = current_parameters->VectorParameters.at(name);
            outX               = param.x();
            outY               = param.y();
            outZ               = param.z();
        } else {
            outX = defX;
            outY = defY;
            outZ = defZ;
        }
    }

    void getParameterColor(const char* name, float defR, float defG, float defB, float defA, float& outR, float& outG, float& outB, float& outA)
    {
        IG_ASSERT(current_parameters != nullptr, "No parameters available!");
        if (current_parameters->ColorParameters.count(name) > 0) {
            IG::Vector4f param = current_parameters->ColorParameters.at(name);
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

void glue_render(const IG::TechniqueVariantShaderSet& shaderSet, const DriverRenderSettings& settings, const IG::ParameterSet* parameterSet, size_t iter)
{
    // Register host thread
    sInterface->registerThread();

    sInterface->shader_set         = shaderSet;
    sInterface->current_iteration  = iter;
    sInterface->current_settings   = settings;
    sInterface->current_parameters = parameterSet;
    sInterface->driver_settings    = convert_settings(settings, iter);

    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.beginShaderLaunch(IG::ShaderType::Device, {});

    ig_render(&sInterface->driver_settings);

    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.endShaderLaunch(IG::ShaderType::Device, {});
}

void glue_setup(const DriverSetupSettings& settings)
{
    IG_ASSERT(sInterface == nullptr, "Only a single instance allowed!");
    sInterface = std::make_unique<Interface>(settings);

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

void glue_tonemap(size_t device, uint32_t* out_pixels, const IG::TonemapSettings& driver_settings)
{
    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.beginShaderLaunch(IG::ShaderType::Tonemap, {});

#ifdef DEVICE_GPU
#if defined(DEVICE_NVVM)
    int dev_id = ANYDSL_DEVICE(ANYDSL_CUDA, (int)device);
#elif defined(DEVICE_AMD)
    int dev_id = ANYDSL_DEVICE(ANYDSL_HSA, (int)device);
#endif
    float* in_pixels            = sInterface->getAOVImage(dev_id, (int)driver_settings.AOV);
    uint32_t* device_out_pixels = sInterface->getTonemapImage(dev_id);
#else
    float* in_pixels            = sInterface->getAOVImage(0, (int)driver_settings.AOV);
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
}

void glue_imageinfo(size_t device, const IG::ImageInfoSettings& driver_settings, IG::ImageInfoOutput& driver_output)
{
    if (sInterface->setup.acquire_stats)
        sInterface->getThreadData()->stats.beginShaderLaunch(IG::ShaderType::ImageInfo, {});

#ifdef DEVICE_GPU
#if defined(DEVICE_NVVM)
    int dev_id = ANYDSL_DEVICE(ANYDSL_CUDA, (int)device);
#elif defined(DEVICE_AMD)
    int dev_id = ANYDSL_DEVICE(ANYDSL_HSA, (int)device);
#endif
    float* in_pixels = sInterface->getAOVImage(dev_id, (int)driver_settings.AOV);
#else
    float* in_pixels            = sInterface->getAOVImage(0, (int)driver_settings.AOV);
#endif

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

void* glue_compileSource(const char* src, const char* function)
{
    size_t len = std::strlen(src);
    int ret    = anydsl_compile(src, (uint32_t)len, OPT_LEVEL);
    if (ret < 0)
        return nullptr;

    return anydsl_lookup_function(ret, function);
}

extern "C" {
IG_EXPORT DriverInterface ig_get_interface()
{
    DriverInterface interface{};
    interface.MajorVersion = IG_VERSION_MAJOR;
    interface.MinorVersion = IG_VERSION_MINOR;

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

IG_EXPORT void ignis_get_work_info(int* width, int* height)
{
    if (sInterface->current_settings.work_width > 0 && sInterface->current_settings.work_height > 0) {
        *width  = (int)sInterface->current_settings.work_width;
        *height = (int)sInterface->current_settings.work_height;
    } else {
        *width  = (int)sInterface->film_width;
        *height = (int)sInterface->film_height;
    }
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

IG_EXPORT void ignis_load_rays(int dev, StreamRay** list)
{
    *list = const_cast<StreamRay*>(sInterface->loadRayList(dev).data());
}

IG_EXPORT void ignis_load_image(int32_t dev, const char* file, float** pixels, int32_t* width, int32_t* height)
{
    auto& img = sInterface->loadImage(dev, file);
    *pixels   = const_cast<float*>(std::get<0>(img).data());
    *width    = (int)std::get<1>(img);
    *height   = (int)std::get<2>(img);
}

IG_EXPORT void ignis_load_buffer(int32_t dev, const char* file, uint8_t** data, int32_t* size)
{
    auto& img = sInterface->loadBuffer(dev, file);
    *data     = const_cast<uint8_t*>(std::get<0>(img).data());
    *size     = (int)std::get<1>(img);
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

IG_EXPORT void ignis_gpu_get_tmp_buffer(int dev, int** buf)
{
    *buf = sInterface->getGPUTemporaryBuffer(dev).data();
}

IG_EXPORT void ignis_gpu_get_ray_begin_end_buffers(int dev, int** ray_begins, int** ray_ends)
{
    auto tuple  = sInterface->getGPURayBeginEndBuffers(dev);
    *ray_begins = std::get<0>(tuple).data();
    *ray_ends   = std::get<1>(tuple).data();
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

IG_EXPORT int ignis_handle_ray_generation(int* id, int size, int xmin, int ymin, int xmax, int ymax)
{
    return sInterface->runRayGenerationShader(id, size, xmin, ymin, xmax, ymax);
}

IG_EXPORT void ignis_handle_miss_shader(int first, int last)
{
    sInterface->runMissShader(first, last);
}

IG_EXPORT void ignis_handle_hit_shader(int entity_id, int first, int last)
{
    sInterface->runHitShader(entity_id, first, last);
}

IG_EXPORT void ignis_handle_advanced_shadow_shader(int first, int last, bool is_hit)
{
    sInterface->runAdvancedShadowShader(first, last, is_hit);
}

IG_EXPORT void ignis_handle_callback_shader(int type)
{
    sInterface->runCallbackShader(type);
}

IG_EXPORT bool ignis_use_advanced_shadow_handling()
{
    return sInterface->useAdvancedShadowHandling();
}

IG_EXPORT bool ignis_is_framebuffer_locked()
{
    return sInterface->isFramebufferLocked();
}

IG_EXPORT void ignis_present(int dev)
{
    if (dev != 0)
        sInterface->present(dev);
}

// Registry stuff

IG_EXPORT int ignis_get_parameter_i32(const char* name, int def)
{
    return sInterface->getParameterInt(name, def);
}

IG_EXPORT float ignis_get_parameter_f32(const char* name, float def)
{
    return sInterface->getParameterFloat(name, def);
}

IG_EXPORT void ignis_get_parameter_vector(const char* name, float defX, float defY, float defZ, float* x, float* y, float* z)
{
    sInterface->getParameterVector(name, defX, defY, defZ, *x, *y, *z);
}

IG_EXPORT void ignis_get_parameter_color(const char* name, float defR, float defG, float defB, float defA, float* r, float* g, float* b, float* a)
{
    sInterface->getParameterColor(name, defR, defG, defB, defA, *r, *g, *b, *a);
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
