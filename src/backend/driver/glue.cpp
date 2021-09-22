#include "Runtime.h"
#include "config/Version.h"
#include "driver/Configuration.h"
#include "driver/Interface.h"
#include "table/SceneDatabase.h"

#include "generated_interface.h"

#include <anydsl_runtime.hpp>

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

#include <iomanip>
#include <mutex>
#include <thread>

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

struct DynTableProxy {
	size_t EntryCount;
	ShallowArray<LookupEntry> LookupEntries;
	ShallowArray<uint8_t> Data;
};

struct SceneDatabaseProxy {
	DynTableProxy Buffers;
	DynTableProxy Textures;
	DynTableProxy Entities;
	DynTableProxy Shapes;
	DynTableProxy Lights;
	DynTableProxy Shaders;
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
		bool scene_ent = false;
		Bvh2Ent bvh2_ent;
		Bvh4Ent bvh4_ent;
		Bvh8Ent bvh8_ent;
		bool database_loaded = false;
		SceneDatabaseProxy database;
		anydsl::Array<int32_t> tmp_buffer;
		anydsl::Array<float> first_primary;
		anydsl::Array<float> second_primary;
		anydsl::Array<float> secondary;
		anydsl::Array<float> film_pixels;
		anydsl::Array<StreamRay> ray_list;
	};
	std::unordered_map<int32_t, DeviceData> devices;

	struct CPUData {
		anydsl::Array<float> cpu_primary;
		anydsl::Array<float> cpu_secondary;
	};
	std::mutex thread_mutex;
	std::unordered_map<std::thread::id, std::unique_ptr<CPUData>> thread_data;

	anydsl::Array<float> host_pixels;
	const IG::SceneDatabase* database;
	size_t film_width;
	size_t film_height;

	IG::uint32 current_iteration;
	Settings current_settings;

	const IG::Ray* ray_list = nullptr; // film_width contains number of rays

	void* ray_generation_shader;
	void* miss_shader;
	void* hit_shader;

	Interface(size_t width, size_t height, const IG::SceneDatabase* database,
			  void* ray_generation_shader,
			  void* miss_shader,
			  void* hit_shader)
		: host_pixels(width * height * 3)
		, database(database)
		, film_width(width)
		, film_height(height)
		, ray_generation_shader(ray_generation_shader)
		, miss_shader(miss_shader)
		, hit_shader(hit_shader)
	{
	}

	~Interface()
	{
	}

	template <typename T>
	anydsl::Array<T>& resize_array(int32_t dev, anydsl::Array<T>& array, size_t size, size_t multiplier)
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

	anydsl::Array<float>& cpu_primary_stream(size_t size)
	{
		return resize_array(0, get_thread_data()->cpu_primary, size, 20);
	}

	anydsl::Array<float>& cpu_secondary_stream(size_t size)
	{
		return resize_array(0, get_thread_data()->cpu_secondary, size, 13);
	}

	anydsl::Array<float>& gpu_first_primary_stream(int32_t dev, size_t size)
	{
		return resize_array(dev, devices[dev].first_primary, size, 20);
	}

	anydsl::Array<float>& gpu_second_primary_stream(int32_t dev, size_t size)
	{
		return resize_array(dev, devices[dev].second_primary, size, 20);
	}

	anydsl::Array<float>& gpu_secondary_stream(int32_t dev, size_t size)
	{
		return resize_array(dev, devices[dev].secondary, size, 13);
	}

	anydsl::Array<int32_t>& gpu_tmp_buffer(int32_t dev, size_t size)
	{
		return resize_array(dev, devices[dev].tmp_buffer, size, 1);
	}

	const Bvh2Ent& load_bvh2_ent(int32_t dev)
	{
		if (devices[dev].scene_ent)
			return devices[dev].bvh2_ent;
		devices[dev].scene_ent		 = true;
		return devices[dev].bvh2_ent = std::move(load_scene_bvh<Node2>(dev));
	}

	const Bvh4Ent& load_bvh4_ent(int32_t dev)
	{
		if (devices[dev].scene_ent)
			return devices[dev].bvh4_ent;
		devices[dev].scene_ent		 = true;
		return devices[dev].bvh4_ent = std::move(load_scene_bvh<Node4>(dev));
	}

	const Bvh8Ent& load_bvh8_ent(int32_t dev)
	{
		if (devices[dev].scene_ent)
			return devices[dev].bvh8_ent;
		devices[dev].scene_ent		 = true;
		return devices[dev].bvh8_ent = std::move(load_scene_bvh<Node8>(dev));
	}

	const anydsl::Array<StreamRay>& load_ray_list(int32_t dev)
	{
		if (devices[dev].ray_list.size() != 0)
			return devices[dev].ray_list;

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

		return devices[dev].ray_list = std::move(copy_to_device(dev, rays));
	}

	template <typename T>
	anydsl::Array<T> copy_to_device(int32_t dev, const T* data, size_t n)
	{
		if (n == 0)
			return anydsl::Array<T>();

		anydsl::Array<T> array(dev, reinterpret_cast<T*>(anydsl_alloc(dev, n * sizeof(T))), n);
		anydsl_copy(0, data, 0, dev, array.data(), 0, sizeof(T) * n);
		return array;
	}

	template <typename T>
	anydsl::Array<T> copy_to_device(int32_t dev, const std::vector<T>& host)
	{
		return copy_to_device(dev, host.data(), host.size());
	}

	template <typename Node>
	BvhProxy<Node, EntityLeaf1> load_scene_bvh(int32_t dev)
	{
		const size_t node_count = database->SceneBVH.Nodes.size() / sizeof(Node);
		const size_t leaf_count = database->SceneBVH.Leaves.size() / sizeof(EntityLeaf1);
		return BvhProxy<Node, EntityLeaf1>{
			std::move(ShallowArray<Node>(dev, reinterpret_cast<const Node*>(database->SceneBVH.Nodes.data()), node_count)),
			std::move(ShallowArray<EntityLeaf1>(dev, reinterpret_cast<const EntityLeaf1*>(database->SceneBVH.Leaves.data()), leaf_count))
		};
	}

	DynTableProxy load_dyntable(int32_t dev, const IG::DynTable& tbl)
	{
		static_assert(sizeof(LookupEntry) == sizeof(IG::LookupEntry), "Expected generated Lookup Entry and internal Lookup Entry to be of same size!");

		DynTableProxy proxy;
		proxy.EntryCount	= tbl.entryCount();
		proxy.LookupEntries = std::move(ShallowArray<LookupEntry>(dev, (LookupEntry*)tbl.lookups().data(), tbl.lookups().size()));
		proxy.Data			= std::move(ShallowArray<uint8_t>(dev, tbl.data().data(), tbl.data().size()));
		return proxy;
	}

	// Load all the data assembled in previous stages to the device
	const SceneDatabaseProxy& load_scene_database(int32_t dev)
	{
		if (devices[dev].database_loaded)
			return devices[dev].database;
		devices[dev].database_loaded = true;

		SceneDatabaseProxy& proxy = devices[dev].database;

		// Load all the buffers to device which are registred by the loading and assembly stage
		// TODO: This might take awhile... dynamic on and off loading would be great...
		proxy.Buffers  = std::move(load_dyntable(dev, database->BufferTable));
		proxy.Textures = std::move(load_dyntable(dev, database->TextureTable));
		proxy.Entities = std::move(load_dyntable(dev, database->EntityTable));
		proxy.Shapes   = std::move(load_dyntable(dev, database->ShapeTable));
		proxy.Lights   = std::move(load_dyntable(dev, database->LightTable));
		proxy.Shaders  = std::move(load_dyntable(dev, database->BsdfTable));
		proxy.BVHs	   = std::move(load_dyntable(dev, database->BVHTable));

		return proxy;
	}

	SceneInfo load_scene_info(int32_t dev)
	{
		IG_UNUSED(dev);

		SceneInfo info;
		info.num_entities = database->EntityTable.entryCount();
		info.num_shapes	  = database->ShapeTable.entryCount();
		info.num_lights	  = database->LightTable.entryCount();
		info.scene_radius = database->SceneRadius;
		return info;
	}

	int run_ray_generation_shader(int capacity, int* id, int xmin, int ymin, int xmax, int ymax)
	{
		using Callback = decltype(ig_ray_generation_shader);
		std::cout << capacity << " " << *id << " " << xmin << " " << ymin << " " << xmax << " " << ymax << std::endl;
		IG_ASSERT(ray_generation_shader != nullptr, "Expected ray generation shader to be valid");
		Callback* callback = (Callback*)ray_generation_shader;
		int ret			   = callback(&current_settings, current_iteration, capacity, id, xmin, ymin, xmax, ymax);
		std::cout << "Hehe" << std::endl;
		return ret;
	}

	void present(int32_t dev)
	{
		anydsl::copy(devices[dev].film_pixels, host_pixels);
	}

	void clear()
	{
		std::fill(host_pixels.begin(), host_pixels.end(), 0.0f);
		for (auto& pair : devices) {
			auto& device_pixels = devices[pair.first].film_pixels;
			if (device_pixels.size())
				anydsl::copy(host_pixels, device_pixels);
		}
	}
};

static std::unique_ptr<Interface> sInterface;

static Settings convert_settings(const DriverRenderSettings* settings)
{
	Settings renderSettings;
	renderSettings.device  = settings->device;
	renderSettings.width   = settings->width;
	renderSettings.height  = settings->height;
	renderSettings.eye.x   = settings->eye[0];
	renderSettings.eye.y   = settings->eye[1];
	renderSettings.eye.z   = settings->eye[2];
	renderSettings.dir.x   = settings->dir[0];
	renderSettings.dir.y   = settings->dir[1];
	renderSettings.dir.z   = settings->dir[2];
	renderSettings.up.x	   = settings->up[0];
	renderSettings.up.y	   = settings->up[1];
	renderSettings.up.z	   = settings->up[2];
	renderSettings.right.x = settings->right[0];
	renderSettings.right.y = settings->right[1];
	renderSettings.right.z = settings->right[2];
	renderSettings.tmin	   = settings->tmin;
	renderSettings.tmax	   = settings->tmax;

	renderSettings.max_path_len = settings->max_path_length;
	renderSettings.debug_mode	= settings->debug_mode;

	return renderSettings;
}

void glue_render(const DriverRenderSettings* settings, IG::uint32 iter)
{
	Settings renderSettings = convert_settings(settings);

	sInterface->ray_list		  = settings->rays;
	sInterface->current_iteration = iter;
	sInterface->current_settings  = renderSettings;

	ig_render(&renderSettings);
}

void glue_setup(const DriverSetupSettings* settings)
{
	sInterface = std::make_unique<Interface>(settings->framebuffer_width, settings->framebuffer_height, settings->database,
											 settings->ray_generation_shader, settings->miss_shader, settings->hit_shader);
}

void glue_shutdown()
{
	sInterface.reset();
}

const float* glue_getframebuffer(int aov)
{
	IG_UNUSED(aov);
	return sInterface->host_pixels.data();
}

void glue_clearframebuffer(int aov)
{
	IG_UNUSED(aov);
	sInterface->clear();
}

inline void get_ray_stream(RayStream& rays, float* ptr, size_t capacity)
{
	rays.id	   = (int*)ptr + 0 * capacity;
	rays.org_x = ptr + 1 * capacity;
	rays.org_y = ptr + 2 * capacity;
	rays.org_z = ptr + 3 * capacity;
	rays.dir_x = ptr + 4 * capacity;
	rays.dir_y = ptr + 5 * capacity;
	rays.dir_z = ptr + 6 * capacity;
	rays.tmin  = ptr + 7 * capacity;
	rays.tmax  = ptr + 8 * capacity;
}

inline void get_primary_stream(PrimaryStream& primary, float* ptr, size_t capacity)
{
	// TODO: This is bad behavior. Need better abstraction
	static_assert(sizeof(int) == 4, "Invalid bytesize configuration");

	get_ray_stream(primary.rays, ptr, capacity);
	primary.ent_id	  = (int*)ptr + 9 * capacity;
	primary.prim_id	  = (int*)ptr + 10 * capacity;
	primary.t		  = ptr + 11 * capacity;
	primary.u		  = ptr + 12 * capacity;
	primary.v		  = ptr + 13 * capacity;
	primary.rnd		  = (unsigned int*)ptr + 14 * capacity;
	primary.mis		  = ptr + 15 * capacity;
	primary.contrib_r = ptr + 16 * capacity;
	primary.contrib_g = ptr + 17 * capacity;
	primary.contrib_b = ptr + 18 * capacity;
	primary.depth	  = (int*)ptr + 19 * capacity;
	primary.size	  = 0;
}

inline void get_secondary_stream(SecondaryStream& secondary, float* ptr, size_t capacity)
{
	get_ray_stream(secondary.rays, ptr, capacity);
	secondary.prim_id = (int*)ptr + 9 * capacity;
	secondary.color_r = ptr + 10 * capacity;
	secondary.color_g = ptr + 11 * capacity;
	secondary.color_b = ptr + 12 * capacity;
	secondary.size	  = 0;
}

extern "C" {
IG_EXPORT DriverInterface ig_get_interface()
{
	DriverInterface interface;
	interface.MajorVersion = IG_VERSION_MAJOR;
	interface.MinorVersion = IG_VERSION_MINOR;

	// Expose SPP
#ifdef CAMERA_LIST
	interface.SPP = 1;
#else
	interface.SPP = 4;
#endif

	interface.Configuration = 0;
// Expose Target
#if defined(DEVICE_DEFAULT)
	interface.Configuration |= IG::IG_C_DEVICE_GENERIC;
#elif defined(DEVICE_AVX)
	interface.Configuration |= IG::IG_C_DEVICE_AVX;
#elif defined(DEVICE_AVX2)
	interface.Configuration |= IG::IG_C_DEVICE_AVX2;
#elif defined(DEVICE_AVX512)
	interface.Configuration |= IG::IG_C_DEVICE_AVX512;
#elif defined(DEVICE_SSE42)
	interface.Configuration |= IG::IG_C_DEVICE_SSE42;
#elif defined(DEVICE_ASIMD)
	interface.Configuration |= IG::IG_C_DEVICE_ASIMD;
#elif defined(DEVICE_NVVM)
	interface.Configuration |= IG::IG_C_DEVICE_NVVM;
#elif defined(DEVICE_NVVM_MEGA)
	interface.Configuration |= IG::IG_C_DEVICE_NVVM_MEGA;
#elif defined(DEVICE_AMD)
	interface.Configuration |= IG::IG_C_DEVICE_AMDGPU;
#elif defined(DEVICE_AMD_MEGA)
	interface.Configuration |= IG::IG_C_DEVICE_AMD_MEGA;
#else
#error No device selected!
#endif

	interface.SetupFunction			   = glue_setup;
	interface.ShutdownFunction		   = glue_shutdown;
	interface.RenderFunction		   = glue_render;
	interface.GetFramebufferFunction   = glue_getframebuffer;
	interface.ClearFramebufferFunction = glue_clearframebuffer;

	return interface;
}

void ignis_get_film_data(int dev, float** pixels, int* width, int* height)
{
	if (dev != 0) {
		auto& device = sInterface->devices[dev];
		if (!device.film_pixels.size()) {
			auto film_size	   = sInterface->film_width * sInterface->film_height * 3;
			auto film_data	   = reinterpret_cast<float*>(anydsl_alloc(dev, sizeof(float) * film_size));
			device.film_pixels = std::move(anydsl::Array<float>(dev, film_data, film_size));
			anydsl::copy(sInterface->host_pixels, device.film_pixels);
		}
		*pixels = device.film_pixels.data();
	} else {
		*pixels = sInterface->host_pixels.data();
	}
	*width	= sInterface->film_width;
	*height = sInterface->film_height;
}

void ignis_load_bvh2_ent(int dev, Node2** nodes, EntityLeaf1** objs)
{
	auto& bvh = sInterface->load_bvh2_ent(dev);
	*nodes	  = const_cast<Node2*>(bvh.nodes.ptr());
	*objs	  = const_cast<EntityLeaf1*>(bvh.objs.ptr());
}

void ignis_load_bvh4_ent(int dev, Node4** nodes, EntityLeaf1** objs)
{
	auto& bvh = sInterface->load_bvh4_ent(dev);
	*nodes	  = const_cast<Node4*>(bvh.nodes.ptr());
	*objs	  = const_cast<EntityLeaf1*>(bvh.objs.ptr());
}

void ignis_load_bvh8_ent(int dev, Node8** nodes, EntityLeaf1** objs)
{
	auto& bvh = sInterface->load_bvh8_ent(dev);
	*nodes	  = const_cast<Node8*>(bvh.nodes.ptr());
	*objs	  = const_cast<EntityLeaf1*>(bvh.objs.ptr());
}

void ignis_load_scene(int dev, SceneDatabase* dtb)
{
	auto assign = [&](const DynTableProxy& tbl) {
		DynTable devtbl;
		devtbl.count  = tbl.EntryCount;
		devtbl.header = const_cast<LookupEntry*>(tbl.LookupEntries.ptr());
		uint64_t size = tbl.Data.size();
		devtbl.size	  = size;
		devtbl.start  = const_cast<uint8_t*>(tbl.Data.ptr());
		return devtbl;
	};

	auto& proxy	  = sInterface->load_scene_database(dev);
	dtb->buffers  = assign(proxy.Buffers);
	dtb->textures = assign(proxy.Textures);
	dtb->entities = assign(proxy.Entities);
	dtb->shapes	  = assign(proxy.Shapes);
	dtb->lights	  = assign(proxy.Lights);
	dtb->shaders  = assign(proxy.Shaders);
	dtb->bvhs	  = assign(proxy.BVHs);
}

void ignis_load_scene_info(int dev, SceneInfo* info)
{
	*info = sInterface->load_scene_info(dev);
}

void ignis_load_rays(int dev, StreamRay** list)
{
	*list = const_cast<StreamRay*>(sInterface->load_ray_list(dev).data());
}

void ignis_cpu_get_primary_stream(PrimaryStream* primary, int size)
{
	auto& array = sInterface->cpu_primary_stream(size);
	get_primary_stream(*primary, array.data(), array.size() / 20);
}

void ignis_cpu_get_secondary_stream(SecondaryStream* secondary, int size)
{
	auto& array = sInterface->cpu_secondary_stream(size);
	get_secondary_stream(*secondary, array.data(), array.size() / 13);
}

void ignis_gpu_get_tmp_buffer(int dev, int** buf, int size)
{
	*buf = sInterface->gpu_tmp_buffer(dev, size).data();
}

void ignis_gpu_get_first_primary_stream(int dev, PrimaryStream* primary, int size)
{
	auto& array = sInterface->gpu_first_primary_stream(dev, size);
	get_primary_stream(*primary, array.data(), array.size() / 20);
}

void ignis_gpu_get_second_primary_stream(int dev, PrimaryStream* primary, int size)
{
	auto& array = sInterface->gpu_second_primary_stream(dev, size);
	get_primary_stream(*primary, array.data(), array.size() / 20);
}

void ignis_gpu_get_secondary_stream(int dev, SecondaryStream* secondary, int size)
{
	auto& array = sInterface->gpu_secondary_stream(dev, size);
	get_secondary_stream(*secondary, array.data(), array.size() / 13);
}

int ignis_handle_ray_generation(int capacity, int* id, int xmin, int ymin, int xmax, int ymax)
{
	return sInterface->run_ray_generation_shader(capacity, id, xmin, ymin, xmax, ymax);
}

void ignis_handle_miss_shader(int first, int last)
{
	//sInterface->run_miss_shader(first, last);
}

void ignis_handle_hit_shader(int entity_id, int first, int last)
{
	//sInterface->run_hit_shader(entity_id, first, last);
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