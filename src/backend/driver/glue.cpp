#include "driver/Configuration.h"
#include "driver/Interface.h"

#include "generated_interface.h"

#include <anydsl_runtime.hpp>

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

template <typename Node, typename Object>
struct Bvh {
	anydsl::Array<Node> nodes;
	anydsl::Array<Object> objs;
};

using Bvh2Ent = Bvh<Node2, EntityLeaf1>;
using Bvh4Ent = Bvh<Node4, EntityLeaf1>;
using Bvh8Ent = Bvh<Node8, EntityLeaf1>;

struct Interface {
	using DeviceImage = std::tuple<anydsl::Array<float>, int32_t, int32_t>;

	struct DeviceData {
		bool scene_ent = false;
		Bvh2Ent bvh2_ent;
		Bvh4Ent bvh4_ent;
		Bvh8Ent bvh8_ent;
		//SceneDatabaseProxy database;
		anydsl::Array<int32_t> tmp_buffer;
		anydsl::Array<float> first_primary;
		anydsl::Array<float> second_primary;
		anydsl::Array<float> secondary;
		anydsl::Array<float> film_pixels;
		//anydsl::Array<Ray> ray_list;
	};
	std::unordered_map<int32_t, DeviceData> devices;

	static thread_local anydsl::Array<float> cpu_primary;
	static thread_local anydsl::Array<float> cpu_secondary;

	anydsl::Array<float> host_pixels;
	const IG::SceneDatabase* database;
	size_t film_width;
	size_t film_height;

	std::vector<std::string> resource_paths;

	Interface(size_t width, size_t height, const IG::SceneDatabase* database)
		: host_pixels(width * height * 3)
		, database(database)
		, film_width(width)
		, film_height(height)

	{
	}

	template <typename T>
	anydsl::Array<T>& resize_array(int32_t dev, anydsl::Array<T>& array, size_t size, size_t multiplier)
	{
		int64_t capacity = (size & ~((1 << 5) - 1)) + 32; // round to 32
		if (array.size() < capacity) {
			auto n = capacity * multiplier;
			array  = std::move(anydsl::Array<T>(dev, reinterpret_cast<T*>(anydsl_alloc(dev, sizeof(T) * n)), n));
		}
		return array;
	}

	anydsl::Array<float>& cpu_primary_stream(size_t size)
	{
		return resize_array(0, cpu_primary, size, 20);
	}

	anydsl::Array<float>& cpu_secondary_stream(size_t size)
	{
		return resize_array(0, cpu_secondary, size, 13);
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
	Bvh<Node, EntityLeaf1> load_scene_bvh(int32_t dev)
	{
		// TODO
		return Bvh<Node, EntityLeaf1>{};
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

thread_local anydsl::Array<float> Interface::cpu_primary;
thread_local anydsl::Array<float> Interface::cpu_secondary;

static std::unique_ptr<Interface> sInterface;

void glue_render(const DriverRenderSettings* settings, IG::uint32 iter)
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

	// TODO
	renderSettings.ray_count = 0;
	renderSettings.rays		 = nullptr;

	ig_render(&renderSettings, (int)iter);
}

void glue_setup(const DriverSetupSettings* settings)
{
	sInterface = std::make_unique<Interface>(settings->framebuffer_width, settings->framebuffer_height, settings->database);
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
	interface.Configuration |= IG::IG_C_DEVICE_NVVM_STREAMING;
#elif defined(DEVICE_NVVM_MEGA)
	interface.Configuration |= IG::IG_C_DEVICE_NVVM_MEGA;
#elif defined(DEVICE_AMD)
	interface.Configuration |= IG::IG_C_DEVICE_AMD_STREAMING;
#elif defined(DEVICE_AMD_MEGA)
	interface.Configuration |= IG::IG_C_DEVICE_AMD_MEGA;
#else
#error No device selected!
#endif

// Expose Camera
#if defined(CAMERA_PERSPECTIVE)
	interface.Configuration |= IG::IG_C_CAMERA_PERSPECTIVE;
#elif defined(CAMERA_ORTHOGONAL)
	interface.Configuration |= IG::IG_C_CAMERA_ORTHOGONAL;
#elif defined(CAMERA_FISHLENS)
	interface.Configuration |= IG::IG_C_CAMERA_FISHLENS;
#elif defined(CAMERA_LIST)
	interface.Configuration |= IG::IG_C_CAMERA_LIST;
#else
#error No camera selected!
#endif

// Expose Integrator
#if defined(RENDERER_PATH)
	interface.Configuration |= IG::IG_C_RENDERER_PATH;
#elif defined(RENDERER_DEBUG)
	interface.Configuration |= IG::IG_C_RENDERER_DEBUG;
#else
#error No renderer selected!
#endif

	interface.SetupFunction			   = glue_setup;
	interface.ShutdownFunction		   = glue_shutdown;
	interface.RenderFunction		   = glue_render;
	interface.GetFramebufferFunction   = glue_getframebuffer;
	interface.ClearFramebufferFunction = glue_clearframebuffer;

	return interface;
}

void ignis_get_film_data(int32_t dev, float** pixels, int32_t* width, int32_t* height)
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

/*void ignis_load_image(int32_t dev, const char* file, float** pixels, int32_t* width, int32_t* height)
{
	auto& img = sInterface->load_image(dev, file);
	*pixels	  = const_cast<float*>(std::get<0>(img).data());
	*width	  = std::get<1>(img);
	*height	  = std::get<2>(img);
}

uint8_t* ignis_load_buffer(int32_t dev, const char* file)
{
	auto& array = sInterface->load_buffer(dev, file);
	return const_cast<uint8_t*>(array.data());
}*/

/*Ray* ignis_load_rays(int32_t dev, size_t n, Ray* rays)
{
	auto& array = sInterface->load_rays(dev, n, rays);
	return const_cast<Ray*>(array.data());
}*/

void ignis_load_bvh2_ent(int32_t dev, Node2** nodes, EntityLeaf1** objs)
{
	auto& bvh = sInterface->load_bvh2_ent(dev);
	*nodes	  = const_cast<Node2*>(bvh.nodes.data());
	*objs	  = const_cast<EntityLeaf1*>(bvh.objs.data());
}

void ignis_load_bvh4_ent(int32_t dev, Node4** nodes, EntityLeaf1** objs)
{
	auto& bvh = sInterface->load_bvh4_ent(dev);
	*nodes	  = const_cast<Node4*>(bvh.nodes.data());
	*objs	  = const_cast<EntityLeaf1*>(bvh.objs.data());
}

void ignis_load_bvh8_ent(int32_t dev, Node8** nodes, EntityLeaf1** objs)
{
	auto& bvh = sInterface->load_bvh8_ent(dev);
	*nodes	  = const_cast<Node8*>(bvh.nodes.data());
	*objs	  = const_cast<EntityLeaf1*>(bvh.objs.data());
}

void ignis_cpu_get_primary_stream(PrimaryStream* primary, int32_t size)
{
	auto& array = sInterface->cpu_primary_stream(size);
	get_primary_stream(*primary, array.data(), array.size() / 20);
}

void ignis_cpu_get_secondary_stream(SecondaryStream* secondary, int32_t size)
{
	auto& array = sInterface->cpu_secondary_stream(size);
	get_secondary_stream(*secondary, array.data(), array.size() / 13);
}

void ignis_gpu_get_tmp_buffer(int32_t dev, int32_t** buf, int32_t size)
{
	*buf = sInterface->gpu_tmp_buffer(dev, size).data();
}

void ignis_gpu_get_first_primary_stream(int32_t dev, PrimaryStream* primary, int32_t size)
{
	auto& array = sInterface->gpu_first_primary_stream(dev, size);
	get_primary_stream(*primary, array.data(), array.size() / 20);
}

void ignis_gpu_get_second_primary_stream(int32_t dev, PrimaryStream* primary, int32_t size)
{
	auto& array = sInterface->gpu_second_primary_stream(dev, size);
	get_primary_stream(*primary, array.data(), array.size() / 20);
}

void ignis_gpu_get_secondary_stream(int32_t dev, SecondaryStream* secondary, int32_t size)
{
	auto& array = sInterface->gpu_secondary_stream(dev, size);
	get_secondary_stream(*secondary, array.data(), array.size() / 13);
}

void ignis_present(int32_t dev)
{
	if (dev != 0)
		sInterface->present(dev);
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