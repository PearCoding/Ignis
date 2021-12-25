#include "Image.h"
#include "Logger.h"
#include "Runtime.h"
#include "Statistics.h"
#include "config/Version.h"
#include "driver/Interface.h"
#include "table/SceneDatabase.h"

#include "generated_interface.h"

#include <anydsl_runtime.hpp>

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

#include <atomic>
#include <iomanip>
#include <mutex>
#include <thread>
#include <type_traits>
#include <variant>

// TODO: It would be great to get the number below automatically
// and/or make it possible to extend it for custom payloads
constexpr size_t MaxRayPayloadComponents = 8;
constexpr size_t RayStreamSize           = 9;
constexpr size_t PrimaryStreamSize       = RayStreamSize + 6 + MaxRayPayloadComponents;
constexpr size_t SecondaryStreamSize     = RayStreamSize + 4;

/// Some arrays do not need new allocations at the host if the data is already provided and properly arranged.
/// However, this assumes that the pointer is always properly aligned!
/// For external devices (e.g., GPU) a standard anydsl array will be used
// TODO: Make a fallback for cases in which the pointer is not aligned
template <typename T>
class ShallowArray {
public:
    inline ShallowArray()
        : device_mem()
        , host_mem(nullptr)
        , device(0)
        , size_(0)
    {
    }

    inline ShallowArray(int32_t dev, const T* ptr, size_t n)
        : device_mem()
        , host_mem(nullptr)
        , device(dev)
        , size_(n)
    {
        if (dev != 0) {
            if (n != 0) {
                device_mem = std::move(anydsl::Array<T>(dev, reinterpret_cast<T*>(anydsl_alloc(dev, sizeof(T) * n)), n));
                anydsl_copy(0, ptr, 0, dev, device_mem.data(), 0, sizeof(T) * n);
            }
        } else {
            host_mem = ptr;
        }
    }

    inline ShallowArray(ShallowArray&&) = default;
    inline ShallowArray& operator=(ShallowArray&&) = default;

    inline ~ShallowArray() = default;

    inline const anydsl::Array<T>& device_data() const { return device_mem; }
    inline const T* host_data() const { return host_mem; }

    inline const T* ptr() const
    {
        if (is_host())
            return host_data();
        else
            return device_data().data();
    }

    inline bool has_data() const { return ptr() != nullptr && size_ > 0; }
    inline bool is_host() const { return host_mem != nullptr && device == 0; }
    inline size_t size() const { return size_; }

private:
    anydsl::Array<T> device_mem;
    const T* host_mem;
    int32_t device;
    size_t size_;
};

template <typename Node, typename Object>
struct BvhProxy {
    ShallowArray<Node> nodes;
    ShallowArray<Object> objs;
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

static inline float safe_rcp(float x)
{
    constexpr float min_rcp = 1e-8f;
    if ((x > 0 ? x : -x) < min_rcp) {
        return std::signbit(x) ? -std::numeric_limits<float>::max() : std::numeric_limits<float>::max();
    } else {
        return 1 / x;
    }
}

struct Interface {
    using DeviceImage = std::tuple<anydsl::Array<float>, int32_t, int32_t>;

    struct DeviceData {
        std::atomic_flag scene_loaded;
        BvhVariant bvh_ent;
        std::atomic_flag database_loaded;
        SceneDatabaseProxy database;
        anydsl::Array<int32_t> tmp_buffer;
        anydsl::Array<float> first_primary;
        anydsl::Array<float> second_primary;
        anydsl::Array<float> first_secondary;
        anydsl::Array<float> second_secondary;
        std::vector<anydsl::Array<float>> aovs;
        anydsl::Array<float> film_pixels;
        anydsl::Array<StreamRay> ray_list;
        anydsl::Array<float>* current_first_primary;
        anydsl::Array<float>* current_second_primary;
        anydsl::Array<float>* current_first_secondary;
        anydsl::Array<float>* current_second_secondary;
        std::unordered_map<std::string, DeviceImage> images;

        inline DeviceData()
            : scene_loaded(ATOMIC_FLAG_INIT)
            , database_loaded(ATOMIC_FLAG_INIT)
        {
            current_first_primary    = &first_primary;
            current_second_primary   = &second_primary;
            current_first_secondary  = &first_secondary;
            current_second_secondary = &second_secondary;
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

    IG::Statistics main_stats;

    inline Interface(const DriverSetupSettings& setup)
        : aovs(setup.aov_count)
        , host_pixels(setup.framebuffer_width * setup.framebuffer_height * 3)
        , database(setup.database)
        , film_width(setup.framebuffer_width)
        , film_height(setup.framebuffer_height)
        , setup(setup)
    {
        for (auto& arr : aovs)
            arr = std::move(anydsl::Array<float>(film_width * film_height * 3));
    }

    inline ~Interface()
    {
    }

    template <typename T>
    inline anydsl::Array<T>& resize_array(int32_t dev, anydsl::Array<T>& array, size_t size, size_t multiplier)
    {
        const auto capacity = (size & ~((1 << 5) - 1)) + 32; // round to 32
        if (array.size() < (int64_t)capacity) {
            auto n = capacity * multiplier;
            array  = std::move(anydsl::Array<T>(dev, reinterpret_cast<T*>(anydsl_alloc(dev, sizeof(T) * n)), n));
        }
        return array;
    }

    inline CPUData* get_thread_data()
    {
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

    inline anydsl::Array<float>& cpu_primary_stream(size_t size)
    {
        return resize_array(0, get_thread_data()->cpu_primary, size, PrimaryStreamSize);
    }

    inline anydsl::Array<float>& cpu_primary_stream_const()
    {
        IG_ASSERT(get_thread_data()->cpu_primary.size() > 0, "Expected cpu primary stream to be initialized");
        return get_thread_data()->cpu_primary;
    }

    inline anydsl::Array<float>& cpu_secondary_stream(size_t size)
    {
        return resize_array(0, get_thread_data()->cpu_secondary, size, SecondaryStreamSize);
    }

    inline anydsl::Array<float>& cpu_secondary_stream_const()
    {
        IG_ASSERT(get_thread_data()->cpu_secondary.size() > 0, "Expected cpu secondary stream to be initialized");
        return get_thread_data()->cpu_secondary;
    }

    inline anydsl::Array<float>& gpu_first_primary_stream(int32_t dev, size_t size)
    {
        return resize_array(dev, *devices[dev].current_first_primary, size, PrimaryStreamSize);
    }

    inline anydsl::Array<float>& gpu_first_primary_stream_const(int32_t dev)
    {
        IG_ASSERT(devices[dev].current_first_primary->size() > 0, "Expected gpu first primary stream to be initialized");
        return *devices[dev].current_first_primary;
    }

    inline anydsl::Array<float>& gpu_second_primary_stream(int32_t dev, size_t size)
    {
        return resize_array(dev, *devices[dev].current_second_primary, size, PrimaryStreamSize);
    }

    inline anydsl::Array<float>& gpu_second_primary_stream_const(int32_t dev)
    {
        IG_ASSERT(devices[dev].current_second_primary->size() > 0, "Expected gpu second primary stream to be initialized");
        return *devices[dev].current_second_primary;
    }

    inline anydsl::Array<float>& gpu_first_secondary_stream(int32_t dev, size_t size)
    {
        return resize_array(dev, *devices[dev].current_first_secondary, size, SecondaryStreamSize);
    }

    inline anydsl::Array<float>& gpu_first_secondary_stream_const(int32_t dev)
    {
        IG_ASSERT(devices[dev].current_first_secondary.size() > 0, "Expected gpu secondary stream to be initialized");
        return *devices[dev].current_first_secondary;
    }

    inline anydsl::Array<float>& gpu_second_secondary_stream(int32_t dev, size_t size)
    {
        return resize_array(dev, *devices[dev].current_second_secondary, size, SecondaryStreamSize);
    }

    inline anydsl::Array<float>& gpu_second_secondary_stream_const(int32_t dev)
    {
        IG_ASSERT(use_advanced_shadow_handling(), "Only use second secondary stream if advanced shadow handling is enabled");
        IG_ASSERT(devices[dev].current_second_secondary.size() > 0, "Expected gpu secondary stream to be initialized");
        return *devices[dev].current_second_secondary;
    }

    inline anydsl::Array<int32_t>& gpu_tmp_buffer(int32_t dev, size_t size)
    {
        return resize_array(dev, devices[dev].tmp_buffer, size, 1);
    }

    inline void gpu_swap_primary_streams(int32_t dev)
    {
        auto& device = devices[dev];
        std::swap(device.current_first_primary, device.current_second_primary);
    }

    inline void gpu_swap_secondary_streams(int32_t dev)
    {
        auto& device = devices[dev];
        std::swap(device.current_first_secondary, device.current_second_secondary);
    }

    template <typename Bvh, typename Node>
    inline const Bvh& load_bvh_ent(int32_t dev)
    {
        auto& device = devices[dev];
        if (!device.scene_loaded.test_and_set())
            device.bvh_ent = std::move(load_scene_bvh<Node>(dev));
        return std::get<Bvh>(device.bvh_ent);
    }

    inline const anydsl::Array<StreamRay>& load_ray_list(int32_t dev)
    {
        auto& device = devices[dev];
        if (device.ray_list.size() != 0)
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

        return device.ray_list = std::move(copy_to_device(dev, rays));
    }

    template <typename T>
    inline anydsl::Array<T> copy_to_device(int32_t dev, const T* data, size_t n)
    {
        if (n == 0)
            return anydsl::Array<T>();

        anydsl::Array<T> array(dev, reinterpret_cast<T*>(anydsl_alloc(dev, n * sizeof(T))), n);
        anydsl_copy(0, data, 0, dev, array.data(), 0, sizeof(T) * n);
        return array;
    }

    template <typename T>
    inline anydsl::Array<T> copy_to_device(int32_t dev, const std::vector<T>& host)
    {
        return copy_to_device(dev, host.data(), host.size());
    }

    inline DeviceImage copy_to_device(int32_t dev, const IG::ImageRgba32& image)
    {
        return DeviceImage(copy_to_device(dev, image.pixels.get(), image.width * image.height * 4), image.width, image.height);
    }

    template <typename Node>
    inline BvhProxy<Node, EntityLeaf1> load_scene_bvh(int32_t dev)
    {
        const size_t node_count = database->SceneBVH.Nodes.size() / sizeof(Node);
        const size_t leaf_count = database->SceneBVH.Leaves.size() / sizeof(EntityLeaf1);
        return BvhProxy<Node, EntityLeaf1>{
            std::move(ShallowArray<Node>(dev, reinterpret_cast<const Node*>(database->SceneBVH.Nodes.data()), node_count)),
            std::move(ShallowArray<EntityLeaf1>(dev, reinterpret_cast<const EntityLeaf1*>(database->SceneBVH.Leaves.data()), leaf_count))
        };
    }

    inline DynTableProxy load_dyntable(int32_t dev, const IG::DynTable& tbl)
    {
        static_assert(sizeof(LookupEntry) == sizeof(IG::LookupEntry), "Expected generated Lookup Entry and internal Lookup Entry to be of same size!");

        DynTableProxy proxy;
        proxy.EntryCount    = tbl.entryCount();
        proxy.LookupEntries = std::move(ShallowArray<LookupEntry>(dev, (LookupEntry*)tbl.lookups().data(), tbl.lookups().size()));
        proxy.Data          = std::move(ShallowArray<uint8_t>(dev, tbl.data().data(), tbl.data().size()));
        return proxy;
    }

    // Load all the data assembled in previous stages to the device
    inline const SceneDatabaseProxy& load_scene_database(int32_t dev)
    {
        auto& device = devices[dev];
        if (device.database_loaded.test_and_set())
            return device.database;

        SceneDatabaseProxy& proxy = device.database;

        proxy.Entities = std::move(load_dyntable(dev, database->EntityTable));
        proxy.Shapes   = std::move(load_dyntable(dev, database->ShapeTable));
        proxy.BVHs     = std::move(load_dyntable(dev, database->BVHTable));

        return proxy;
    }

    inline SceneInfo load_scene_info(int32_t dev)
    {
        IG_UNUSED(dev);

        SceneInfo info;
        info.num_entities = database->EntityTable.entryCount();
        return info;
    }

    inline const DeviceImage& load_image(int32_t dev, const std::string& filename)
    {
        std::lock_guard<std::mutex> _guard(thread_mutex);

        auto& images = devices[dev].images;
        auto it      = images.find(filename);
        if (it != images.end())
            return it->second;

        IG_LOG(IG::L_DEBUG) << "Loading image " << filename << std::endl;
        try {
            return images[filename] = std::move(copy_to_device(dev, IG::ImageRgba32::load(filename)));
        } catch (const IG::ImageLoadException& e) {
            IG_LOG(IG::L_ERROR) << e.what() << std::endl;
            return images[filename] = std::move(copy_to_device(dev, IG::ImageRgba32()));
        }
    }

    inline int run_ray_generation_shader(int* id, int size, int xmin, int ymin, int xmax, int ymax)
    {
        if (setup.acquire_stats)
            get_thread_data()->stats.beginShaderLaunch(IG::ShaderType::RayGeneration, {});

        using Callback = decltype(ig_ray_generation_shader);
        IG_ASSERT(setup.ray_generation_shader != nullptr, "Expected ray generation shader to be valid");
        auto callback = (Callback*)setup.ray_generation_shader;
        const int ret = callback(&current_settings, current_iteration, id, size, xmin, ymin, xmax, ymax);

        if (setup.acquire_stats)
            get_thread_data()->stats.endShaderLaunch(IG::ShaderType::RayGeneration, {});
        return ret;
    }

    inline void run_miss_shader(int first, int last)
    {
        if (setup.acquire_stats)
            get_thread_data()->stats.beginShaderLaunch(IG::ShaderType::Miss, {});

        using Callback = decltype(ig_miss_shader);
        IG_ASSERT(setup.miss_shader != nullptr, "Expected miss shader to be valid");
        auto callback = (Callback*)setup.miss_shader;
        callback(&current_settings, first, last);

        if (setup.acquire_stats)
            get_thread_data()->stats.endShaderLaunch(IG::ShaderType::Miss, {});
    }

    inline void run_hit_shader(int entity_id, int first, int last)
    {
        if (setup.acquire_stats)
            get_thread_data()->stats.beginShaderLaunch(IG::ShaderType::Hit, entity_id);

        using Callback = decltype(ig_hit_shader);
        IG_ASSERT(entity_id >= 0 && entity_id < (int)setup.hit_shaders.size(), "Expected entity id for hit shaders to be valid");
        void* hit_shader = setup.hit_shaders[entity_id];
        IG_ASSERT(setup.hit_shader != nullptr, "Expected hit shader to be valid");
        auto callback = (Callback*)hit_shader;
        callback(&current_settings, entity_id, first, last);

        if (setup.acquire_stats)
            get_thread_data()->stats.endShaderLaunch(IG::ShaderType::Hit, entity_id);
    }

    inline bool use_advanced_shadow_handling()
    {
        return setup.advanced_shadow_hit_shader != nullptr && setup.advanced_shadow_miss_shader != nullptr;
    }

    inline void run_advanced_shadow_shader(int first, int last, bool is_hit)
    {
        IG_ASSERT(use_advanced_shadow_handling(), "Expected advanced shadow shader only be called if it is enabled!");

        if (is_hit) {
            if (setup.acquire_stats)
                get_thread_data()->stats.beginShaderLaunch(IG::ShaderType::AdvancedShadowHit, {});

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(setup.advanced_shadow_hit_shader != nullptr, "Expected miss shader to be valid");
            auto callback = (Callback*)setup.advanced_shadow_hit_shader;
            callback(&current_settings, first, last);

            if (setup.acquire_stats)
                get_thread_data()->stats.endShaderLaunch(IG::ShaderType::AdvancedShadowHit, {});
        } else {
            if (setup.acquire_stats)
                get_thread_data()->stats.beginShaderLaunch(IG::ShaderType::AdvancedShadowMiss, {});

            using Callback = decltype(ig_advanced_shadow_shader);
            IG_ASSERT(setup.advanced_shadow_miss_shader != nullptr, "Expected miss shader to be valid");
            auto callback = (Callback*)setup.advanced_shadow_miss_shader;
            callback(&current_settings, first, last);

            if (setup.acquire_stats)
                get_thread_data()->stats.endShaderLaunch(IG::ShaderType::AdvancedShadowMiss, {});
        }
    }

    inline float* get_film_image(int32_t dev)
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

    inline float* get_aov_image(int32_t dev, int32_t id)
    {
        if (id == 0) // Id = 0 is the actual framebuffer
            return get_film_image(dev);

        int32_t index = id - 1;
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

    inline void present(int32_t dev)
    {
        for (size_t id = 0; id < aovs.size(); ++id)
            anydsl::copy(devices[dev].aovs[id], aovs[id]);

        anydsl::copy(devices[dev].film_pixels, host_pixels);
    }

    inline void clear()
    {
        std::fill(host_pixels.begin(), host_pixels.end(), 0.0f);
        for (auto& pair : devices) {
            auto& device_pixels = devices[pair.first].film_pixels;
            if (device_pixels.size())
                anydsl::copy(host_pixels, device_pixels);
        }

        // TODO: Be more selective?
        for (size_t id = 0; id < aovs.size(); ++id) {
            std::fill(aovs[id].begin(), aovs[id].end(), 0.0f);
            for (auto& pair : devices) {
                if (devices[pair.first].aovs.empty())
                    continue;
                auto& device_pixels = devices[pair.first].aovs[id];
                if (device_pixels.size())
                    anydsl::copy(aovs[id], device_pixels);
            }
        }
    }

    inline IG::Statistics* get_full_stats()
    {
        main_stats.reset();
        for (const auto& pair : thread_data)
            main_stats.add(pair.second->stats);

        return &main_stats;
    }
};

static std::unique_ptr<Interface> sInterface;

static Settings convert_settings(const DriverRenderSettings* settings)
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

    renderSettings.debug_mode = settings->debug_mode;

    return renderSettings;
}

void glue_render(const DriverRenderSettings* settings, IG::uint32 iter)
{
    Settings renderSettings = convert_settings(settings);

    sInterface->ray_list          = settings->rays;
    sInterface->current_iteration = iter;
    sInterface->current_settings  = renderSettings;

    if (sInterface->setup.acquire_stats)
        sInterface->get_thread_data()->stats.beginShaderLaunch(IG::ShaderType::Device, {});

    ig_render(&renderSettings);

    if (sInterface->setup.acquire_stats)
        sInterface->get_thread_data()->stats.endShaderLaunch(IG::ShaderType::Device, {});
}

void glue_setup(const DriverSetupSettings* settings)
{
    sInterface = std::make_unique<Interface>(*settings);
}

void glue_shutdown()
{
    sInterface.reset();
}

const float* glue_getframebuffer(int aov)
{
    if (aov <= 0 || (size_t)aov > sInterface->aovs.size())
        return sInterface->host_pixels.data();
    else
        return sInterface->aovs[aov - 1].data();
}

void glue_clearframebuffer(int aov)
{
    // TODO: Be more selective
    IG_UNUSED(aov);
    sInterface->clear();
}

const IG::Statistics* glue_getstatistics()
{
    return sInterface->get_full_stats();
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
    interface.GetFramebufferFunction   = glue_getframebuffer;
    interface.ClearFramebufferFunction = glue_clearframebuffer;
    interface.GetStatisticsFunction    = glue_getstatistics;

    return interface;
}

void ignis_get_film_data(int dev, float** pixels, int* width, int* height)
{
    *pixels = sInterface->get_film_image(dev);
    *width  = sInterface->film_width;
    *height = sInterface->film_height;
}

void ignis_get_aov_image(int dev, int id, float** aov_pixels)
{
    *aov_pixels = sInterface->get_aov_image(dev, id);
}

void ignis_load_bvh2_ent(int dev, Node2** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->load_bvh_ent<Bvh2Ent, Node2>(dev);
    *nodes    = const_cast<Node2*>(bvh.nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.objs.ptr());
}

void ignis_load_bvh4_ent(int dev, Node4** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->load_bvh_ent<Bvh4Ent, Node4>(dev);
    *nodes    = const_cast<Node4*>(bvh.nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.objs.ptr());
}

void ignis_load_bvh8_ent(int dev, Node8** nodes, EntityLeaf1** objs)
{
    auto& bvh = sInterface->load_bvh_ent<Bvh8Ent, Node8>(dev);
    *nodes    = const_cast<Node8*>(bvh.nodes.ptr());
    *objs     = const_cast<EntityLeaf1*>(bvh.objs.ptr());
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

    auto& proxy   = sInterface->load_scene_database(dev);
    dtb->entities = assign(proxy.Entities);
    dtb->shapes   = assign(proxy.Shapes);
    dtb->bvhs     = assign(proxy.BVHs);
}

void ignis_load_scene_info(int dev, SceneInfo* info)
{
    *info = sInterface->load_scene_info(dev);
}

void ignis_load_rays(int dev, StreamRay** list)
{
    *list = const_cast<StreamRay*>(sInterface->load_ray_list(dev).data());
}

void ignis_load_image(int32_t dev, const char* file, float** pixels, int32_t* width, int32_t* height)
{
    auto& img = sInterface->load_image(dev, file);
    *pixels   = const_cast<float*>(std::get<0>(img).data());
    *width    = std::get<1>(img);
    *height   = std::get<2>(img);
}

void ignis_cpu_get_primary_stream(PrimaryStream* primary, int size)
{
    auto& array = sInterface->cpu_primary_stream(size);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_cpu_get_primary_stream_const(PrimaryStream* primary)
{
    auto& array = sInterface->cpu_primary_stream_const();
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_cpu_get_secondary_stream(SecondaryStream* secondary, int size)
{
    auto& array = sInterface->cpu_secondary_stream(size);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_cpu_get_secondary_stream_const(SecondaryStream* secondary)
{
    auto& array = sInterface->cpu_secondary_stream_const();
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_gpu_get_tmp_buffer(int dev, int** buf, int size)
{
    *buf = sInterface->gpu_tmp_buffer(dev, size).data();
}

void ignis_gpu_get_first_primary_stream(int dev, PrimaryStream* primary, int size)
{
    auto& array = sInterface->gpu_first_primary_stream(dev, size);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_gpu_get_first_primary_stream_const(int dev, PrimaryStream* primary)
{
    auto& array = sInterface->gpu_first_primary_stream_const(dev);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_gpu_get_second_primary_stream(int dev, PrimaryStream* primary, int size)
{
    auto& array = sInterface->gpu_second_primary_stream(dev, size);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_gpu_get_second_primary_stream_const(int dev, PrimaryStream* primary)
{
    auto& array = sInterface->gpu_second_primary_stream_const(dev);
    get_primary_stream(*primary, array.data(), array.size() / PrimaryStreamSize, PrimaryStreamSize);
}

void ignis_gpu_get_first_secondary_stream(int dev, SecondaryStream* secondary, int size)
{
    auto& array = sInterface->gpu_first_secondary_stream(dev, size);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_gpu_get_first_secondary_stream_const(int dev, SecondaryStream* secondary)
{
    auto& array = sInterface->gpu_first_secondary_stream_const(dev);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_gpu_get_second_secondary_stream(int dev, SecondaryStream* secondary, int size)
{
    auto& array = sInterface->gpu_second_secondary_stream(dev, size);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_gpu_get_second_secondary_stream_const(int dev, SecondaryStream* secondary)
{
    auto& array = sInterface->gpu_second_secondary_stream_const(dev);
    get_secondary_stream(*secondary, array.data(), array.size() / SecondaryStreamSize);
}

void ignis_gpu_swap_primary_streams(int dev)
{
    sInterface->gpu_swap_primary_streams(dev);
}

void ignis_gpu_swap_secondary_streams(int dev)
{
    sInterface->gpu_swap_secondary_streams(dev);
}

int ignis_handle_ray_generation(int* id, int size, int xmin, int ymin, int xmax, int ymax)
{
    return sInterface->run_ray_generation_shader(id, size, xmin, ymin, xmax, ymax);
}

void ignis_handle_miss_shader(int first, int last)
{
    sInterface->run_miss_shader(first, last);
}

void ignis_handle_hit_shader(int entity_id, int first, int last)
{
    sInterface->run_hit_shader(entity_id, first, last);
}

void ignis_handle_advanced_shadow_shader(int first, int last, bool is_hit)
{
    sInterface->run_advanced_shadow_shader(first, last, is_hit);
}

bool ignis_use_advanced_shadow_handling()
{
    return sInterface->use_advanced_shadow_handling();
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