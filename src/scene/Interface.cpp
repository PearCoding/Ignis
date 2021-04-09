#include <fstream>
#include <unordered_map>

#include <anydsl_runtime.hpp>

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

#include "Buffer.h"
#include "Image.h"
#include "Logger.h"
#include "bvh/BVH.h"
#include "mesh/TriMesh.h"

#include "generated_interface.h"

using namespace IG;

template <typename Node, typename Object>
struct Bvh {
	anydsl::Array<Node> nodes;
	anydsl::Array<Object> objs;
};

using Bvh2Tri1 = Bvh<Node2, Tri1>;
using Bvh4Tri4 = Bvh<Node4, Tri4>;
using Bvh8Tri4 = Bvh<Node8, Tri4>;

using Bvh2Bbox = Bvh<Node2, TaggedBBox1>;
using Bvh4Bbox = Bvh<Node4, TaggedBBox1>;
using Bvh8Bbox = Bvh<Node8, TaggedBBox1>;

struct Interface {
	using DeviceImage = std::tuple<anydsl::Array<float>, int32_t, int32_t>;

	struct DeviceData {
		std::unordered_map<std::string, Bvh2Tri1> bvh2_tri1;
		std::unordered_map<std::string, Bvh4Tri4> bvh4_tri4;
		std::unordered_map<std::string, Bvh8Tri4> bvh8_tri4;
		std::unordered_map<std::string, Bvh2Bbox> bvh2_bbox;
		std::unordered_map<std::string, Bvh4Bbox> bvh4_bbox;
		std::unordered_map<std::string, Bvh8Bbox> bvh8_bbox;
		std::unordered_map<std::string, anydsl::Array<uint8_t>> buffers;
		std::unordered_map<std::string, DeviceImage> images;
		anydsl::Array<int32_t> tmp_buffer;
		anydsl::Array<float> first_primary;
		anydsl::Array<float> second_primary;
		anydsl::Array<float> secondary;
		anydsl::Array<float> film_pixels;
		anydsl::Array<Ray> ray_list;
	};
	std::unordered_map<int32_t, DeviceData> devices;

	static thread_local anydsl::Array<float> cpu_primary;
	static thread_local anydsl::Array<float> cpu_secondary;

	anydsl::Array<float> host_pixels;
	size_t film_width;
	size_t film_height;

	std::vector<std::string> resource_paths;

	Interface(size_t width, size_t height)
		: host_pixels(width * height * 3)
		, film_width(width)
		, film_height(height)

	{
	}

	inline std::string lookupPath(const std::string& path) const
	{
		auto exists = [](const std::string& p) {
			std::ifstream f(p.c_str());
			return f.good();
		};

		// If empty, do not bother any further
		if (path.empty())
			return path;

		// If exists, nothing to escape
		if (exists(path))
			return path;

		// Check if one of the lookup paths work
		for (const std::string& dirs : resource_paths) {
			const std::string p = dirs + "/" + path;
			if (exists(p))
				return p;
		}

		// Nothing found
		return path;
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

	const Bvh2Tri1& load_bvh2_tri1(int32_t dev, const std::string& filename)
	{
		auto& bvh2_tri1 = devices[dev].bvh2_tri1;
		auto it			= bvh2_tri1.find(filename);
		if (it != bvh2_tri1.end())
			return it->second;
		return bvh2_tri1[filename] = std::move(load_tri_bvh<Node2, Tri1>(dev, filename));
	}

	const Bvh4Tri4& load_bvh4_tri4(int32_t dev, const std::string& filename)
	{
		auto& bvh4_tri4 = devices[dev].bvh4_tri4;
		auto it			= bvh4_tri4.find(filename);
		if (it != bvh4_tri4.end())
			return it->second;
		return bvh4_tri4[filename] = std::move(load_tri_bvh<Node4, Tri4>(dev, filename));
	}

	const Bvh8Tri4& load_bvh8_tri4(int32_t dev, const std::string& filename)
	{
		auto& bvh8_tri4 = devices[dev].bvh8_tri4;
		auto it			= bvh8_tri4.find(filename);
		if (it != bvh8_tri4.end())
			return it->second;
		return bvh8_tri4[filename] = std::move(load_tri_bvh<Node8, Tri4>(dev, filename));
	}

	const Bvh2Bbox& load_bvh2_bbox(int32_t dev, const std::string& filename)
	{
		auto& bvh2_bbox = devices[dev].bvh2_bbox;
		auto it			= bvh2_bbox.find(filename);
		if (it != bvh2_bbox.end())
			return it->second;
		return bvh2_bbox[filename] = std::move(load_scene_bvh<Node2>(dev, filename));
	}

	const Bvh4Bbox& load_bvh4_bbox(int32_t dev, const std::string& filename)
	{
		auto& bvh4_bbox = devices[dev].bvh4_bbox;
		auto it			= bvh4_bbox.find(filename);
		if (it != bvh4_bbox.end())
			return it->second;
		return bvh4_bbox[filename] = std::move(load_scene_bvh<Node4>(dev, filename));
	}

	const Bvh8Bbox& load_bvh8_bbox(int32_t dev, const std::string& filename)
	{
		auto& bvh8_bbox = devices[dev].bvh8_bbox;
		auto it			= bvh8_bbox.find(filename);
		if (it != bvh8_bbox.end())
			return it->second;
		return bvh8_bbox[filename] = std::move(load_scene_bvh<Node8>(dev, filename));
	}

	template <typename T>
	anydsl::Array<T> copy_to_device(int32_t dev, const T* data, size_t n)
	{
		if (n == 0) {
			IG_LOG(L_ERROR) << "Trying to copy an empty array to device!" << std::endl;
			return anydsl::Array<T>();
		}

		anydsl::Array<T> array(dev, reinterpret_cast<T*>(anydsl_alloc(dev, n * sizeof(T))), n);
		anydsl_copy(0, data, 0, dev, array.data(), 0, sizeof(T) * n);
		return array;
	}

	template <typename T>
	anydsl::Array<T> copy_to_device(int32_t dev, const std::vector<T>& host)
	{
		return copy_to_device(dev, host.data(), host.size());
	}

	DeviceImage copy_to_device(int32_t dev, const ImageRgba32& img)
	{
		return DeviceImage{ copy_to_device(dev, img.pixels.get(), img.width * img.height * 4),
							(int32_t)img.width, (int32_t)img.height };
	}

	template <typename Node, typename Tri>
	Bvh<Node, Tri> load_tri_bvh(int32_t dev, const std::string& filename)
	{
		const std::string lpath = lookupPath(filename);
		std::ifstream is(lpath, std::ios::binary);
		if (!is)
			IG_LOG(L_ERROR) << "Cannot open shape BVH '" << lpath << "'" << std::endl;
		do {
			size_t node_size = 0, tri_size = 0;
			is.read((char*)&node_size, sizeof(uint32_t));
			is.read((char*)&tri_size, sizeof(uint32_t));
			if (node_size == sizeof(Node) && tri_size == sizeof(Tri)) {
				IG_LOG(L_INFO) << "Loaded shape BVH file '" << lpath << "'" << std::endl;
				std::vector<Node> nodes;
				std::vector<Tri> tris;
				IO::read_buffer(is, nodes);
				IO::read_buffer(is, tris);
				IG_LOG(L_INFO) << ">> " << nodes.size() << " nodes and " << tris.size() << " triangles" << std::endl;
				return Bvh<Node, Tri>{ std::move(copy_to_device(dev, nodes)), std::move(copy_to_device(dev, tris)) };
			}
			IO::skip_buffer(is);
			IO::skip_buffer(is);
		} while (!is.eof() && is);
		IG_LOG(L_ERROR) << "Invalid shape BVH file" << std::endl;
		return Bvh<Node, Tri>{};
	}

	template <typename Node>
	Bvh<Node, TaggedBBox1> load_scene_bvh(int32_t dev, const std::string& filename)
	{
		const std::string lpath = lookupPath(filename);
		std::ifstream is(lpath, std::ios::binary);
		if (!is)
			IG_LOG(L_ERROR) << "Cannot open scene BVH '" << lpath << "'" << std::endl;
		do {
			size_t node_size = 0, obj_size = 0;
			is.read((char*)&node_size, sizeof(uint32_t));
			is.read((char*)&obj_size, sizeof(uint32_t));
			if (node_size == sizeof(Node) && obj_size == sizeof(TaggedBBox1)) {
				IG_LOG(L_INFO) << "Loaded scene BVH file '" << lpath << "'" << std::endl;
				std::vector<Node> nodes;
				std::vector<TaggedBBox1> objs;
				IO::read_buffer(is, nodes);
				IO::read_buffer(is, objs);
				IG_LOG(L_INFO) << ">> " << nodes.size() << " nodes and " << objs.size() << " objects" << std::endl;
				return Bvh<Node, TaggedBBox1>{ std::move(copy_to_device(dev, nodes)), std::move(copy_to_device(dev, objs)) };
			}
			IO::skip_buffer(is);
			IO::skip_buffer(is);
		} while (!is.eof() && is);
		IG_LOG(L_ERROR) << "Invalid scene BVH file" << std::endl;
		return Bvh<Node, TaggedBBox1>{};
	}

	const anydsl::Array<uint8_t>& load_buffer(int32_t dev, const std::string& filename)
	{
		auto& buffers = devices[dev].buffers;
		auto it		  = buffers.find(filename);
		if (it != buffers.end())
			return it->second;
		const std::string lpath = lookupPath(filename);
		std::ifstream is(lpath, std::ios::binary);
		if (!is)
			IG_LOG(L_ERROR) << "Cannot open buffer '" << lpath << "'" << std::endl;
		std::vector<uint8_t> vector;
		IO::read_buffer(is, vector);
		IG_LOG(L_INFO) << "Loaded buffer '" << lpath << "'" << std::endl;
		return buffers[filename] = std::move(copy_to_device(dev, vector));
	}

	const anydsl::Array<Ray>& load_rays(int32_t dev, size_t n, Ray* rays)
	{
		return devices[dev].ray_list = std::move(copy_to_device(dev, rays, n));
	}

	const DeviceImage& load_image(int32_t dev, const std::string& filename)
	{
		auto& images = devices[dev].images;
		auto it		 = images.find(filename);
		if (it != images.end())
			return it->second;
		const std::string lpath = lookupPath(filename);
		ImageRgba32 img			= ImageRgba32::load(lpath);
		if (!img.isValid())
			IG_LOG(L_ERROR) << "Cannot load image '" << lpath << "'" << std::endl;
		IG_LOG(L_INFO) << "Loaded image '" << lpath << "'" << std::endl;
		return images[filename] = std::move(copy_to_device(dev, img));
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

static std::unique_ptr<Interface> interface;

namespace IG {
void setup_interface(size_t width, size_t height)
{
	interface.reset(new Interface(width, height));

	// Force flush to zero mode for denormals
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
	_mm_setcsr(_mm_getcsr() | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
#endif
}

void get_interface(size_t& width, size_t& height)
{
	if (interface) {
		width  = interface->film_width;
		height = interface->film_height;
	} else {
		width  = 0;
		height = 0;
	}
}

void cleanup_interface()
{
	interface.reset();
}

float* get_pixels()
{
	return interface->host_pixels.data();
}

void clear_pixels()
{
	return interface->clear();
}

void add_resource_path(const char* path)
{
	if (std::find(interface->resource_paths.begin(), interface->resource_paths.end(), path) == interface->resource_paths.end())
		interface->resource_paths.push_back(path);
}
} // namespace IG

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

void ignis_get_film_data(int32_t dev, float** pixels, int32_t* width, int32_t* height)
{
	if (dev != 0) {
		auto& device = interface->devices[dev];
		if (!device.film_pixels.size()) {
			auto film_size	   = interface->film_width * interface->film_height * 3;
			auto film_data	   = reinterpret_cast<float*>(anydsl_alloc(dev, sizeof(float) * film_size));
			device.film_pixels = std::move(anydsl::Array<float>(dev, film_data, film_size));
			anydsl::copy(interface->host_pixels, device.film_pixels);
		}
		*pixels = device.film_pixels.data();
	} else {
		*pixels = interface->host_pixels.data();
	}
	*width	= interface->film_width;
	*height = interface->film_height;
}

void ignis_load_image(int32_t dev, const char* file, float** pixels, int32_t* width, int32_t* height)
{
	auto& img = interface->load_image(dev, file);
	*pixels	  = const_cast<float*>(std::get<0>(img).data());
	*width	  = std::get<1>(img);
	*height	  = std::get<2>(img);
}

uint8_t* ignis_load_buffer(int32_t dev, const char* file)
{
	auto& array = interface->load_buffer(dev, file);
	return const_cast<uint8_t*>(array.data());
}

Ray* ignis_load_rays(int32_t dev, size_t n, Ray* rays)
{
	auto& array = interface->load_rays(dev, n, rays);
	return const_cast<Ray*>(array.data());
}

void ignis_load_bvh2_tri1(int32_t dev, const char* file, Node2** nodes, Tri1** tris)
{
	auto& bvh = interface->load_bvh2_tri1(dev, file);
	*nodes	  = const_cast<Node2*>(bvh.nodes.data());
	*tris	  = const_cast<Tri1*>(bvh.objs.data());
}

void ignis_load_bvh4_tri4(int32_t dev, const char* file, Node4** nodes, Tri4** tris)
{
	auto& bvh = interface->load_bvh4_tri4(dev, file);
	*nodes	  = const_cast<Node4*>(bvh.nodes.data());
	*tris	  = const_cast<Tri4*>(bvh.objs.data());
}

void ignis_load_bvh8_tri4(int32_t dev, const char* file, Node8** nodes, Tri4** tris)
{
	auto& bvh = interface->load_bvh8_tri4(dev, file);
	*nodes	  = const_cast<Node8*>(bvh.nodes.data());
	*tris	  = const_cast<Tri4*>(bvh.objs.data());
}

void ignis_load_bvh2_bbox(int32_t dev, const char* file, Node2** nodes, TaggedBBox1** objs)
{
	auto& bvh = interface->load_bvh2_bbox(dev, file);
	*nodes	  = const_cast<Node2*>(bvh.nodes.data());
	*objs	  = const_cast<TaggedBBox1*>(bvh.objs.data());
}

void ignis_load_bvh4_bbox(int32_t dev, const char* file, Node4** nodes, TaggedBBox1** objs)
{
	auto& bvh = interface->load_bvh4_bbox(dev, file);
	*nodes	  = const_cast<Node4*>(bvh.nodes.data());
	*objs	  = const_cast<TaggedBBox1*>(bvh.objs.data());
}

void ignis_load_bvh8_bbox(int32_t dev, const char* file, Node8** nodes, TaggedBBox1** objs)
{
	auto& bvh = interface->load_bvh8_bbox(dev, file);
	*nodes	  = const_cast<Node8*>(bvh.nodes.data());
	*objs	  = const_cast<TaggedBBox1*>(bvh.objs.data());
}

void ignis_cpu_get_primary_stream(PrimaryStream* primary, int32_t size)
{
	auto& array = interface->cpu_primary_stream(size);
	get_primary_stream(*primary, array.data(), array.size() / 20);
}

void ignis_cpu_get_secondary_stream(SecondaryStream* secondary, int32_t size)
{
	auto& array = interface->cpu_secondary_stream(size);
	get_secondary_stream(*secondary, array.data(), array.size() / 13);
}

void ignis_gpu_get_tmp_buffer(int32_t dev, int32_t** buf, int32_t size)
{
	*buf = interface->gpu_tmp_buffer(dev, size).data();
}

void ignis_gpu_get_first_primary_stream(int32_t dev, PrimaryStream* primary, int32_t size)
{
	auto& array = interface->gpu_first_primary_stream(dev, size);
	get_primary_stream(*primary, array.data(), array.size() / 20);
}

void ignis_gpu_get_second_primary_stream(int32_t dev, PrimaryStream* primary, int32_t size)
{
	auto& array = interface->gpu_second_primary_stream(dev, size);
	get_primary_stream(*primary, array.data(), array.size() / 20);
}

void ignis_gpu_get_secondary_stream(int32_t dev, SecondaryStream* secondary, int32_t size)
{
	auto& array = interface->gpu_secondary_stream(dev, size);
	get_secondary_stream(*secondary, array.data(), array.size() / 13);
}

void ignis_present(int32_t dev)
{
	if (dev != 0)
		interface->present(dev);
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

} // extern "C"