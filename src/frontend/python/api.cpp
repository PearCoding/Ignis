#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Camera.h"
#include "Interface.h"
#include "generated_interface.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

static inline float safe_rcp(float x)
{
	constexpr float min_rcp = 1e-8f;
	if ((x > 0 ? x : -x) < min_rcp) {
		return std::signbit(x) ? -std::numeric_limits<float>::max() : std::numeric_limits<float>::max();
	} else {
		return 1 / x;
	}
}

static inline Ray construct_ray(const Vec3& org, const Vec3& dir, float tmin, float tmax) { return Ray{ org, dir, Vec3{ 0, 0, 0 }, Vec3{ 0, 0, 0 }, tmin, tmax }; }
static inline Vec3 array_vec3(py::array_t<float> arr)
{
	auto r = arr.unchecked<1>();
	return Vec3{ r(0), r(1), r(2) };
}

PYBIND11_MODULE(pyignis, m)
{
	m.doc() = R"pbdoc(
        Ignis python scene plugin
        -----------------------
        .. currentmodule:: pyignis
        .. autosummary::
           :toctree: _generate
    )pbdoc";

	m.def("spp", &get_spp, "Return number of spp per iteration.");

	m.def("default_settings", &get_default_settings, "Return default settings.");

	m.def("setup", &IG::setup_interface, "Setup initial interface.");

	m.def("cleanup", &IG::cleanup_interface, "Cleanup interface.");

	m.def("clear_pixels", &IG::clear_pixels, "Cleanup internal pixel buffer.");

	m.def("render", &render, "Render scene based on the given settings.");

	m.def("pixels", []() {
		size_t width, height;
		IG::get_interface(width, height);
		return py::memoryview::from_buffer(
			IG::get_pixels(),												// buffer pointer
			{ height, width, 3ul },											// shape (rows, cols)
			{ sizeof(float) * width * 3, sizeof(float) * 3, sizeof(float) } // strides in bytes
		);
	});

	m.def(
		"trace", [](const std::vector<Ray>& rays) {
			std::vector<Ray> copy = rays;
			for (auto& ray : copy) {
				ray.inv_dir.x = safe_rcp(ray.dir.x);
				ray.inv_dir.y = safe_rcp(ray.dir.y);
				ray.inv_dir.z = safe_rcp(ray.dir.z);

				ray.inv_org.x = -(ray.org.x * ray.inv_dir.x);
				ray.inv_org.y = -(ray.org.y * ray.inv_dir.y);
				ray.inv_org.z = -(ray.org.z * ray.inv_dir.z);
			}

			Settings settings;
			settings.ray_count = copy.size();
			settings.rays	   = copy.data();
			settings.width	   = copy.size();
			settings.height	   = 1;
			settings.eye	   = Vec3{ 0, 0, 0 };
			settings.dir	   = Vec3{ 0, 0, 1 };
			settings.up		   = Vec3{ 0, 1, 0 };
			settings.right	   = Vec3{ 1, 0, 0 };
			render(&settings, 1);

			std::vector<float> res(copy.size());
			float* data = IG::get_pixels();
			for (size_t i = 0; i < copy.size(); ++i) {
				res.push_back(data[3 * i + 0]);
				res.push_back(data[3 * i + 1]);
				res.push_back(data[3 * i + 2]);
			}
			return res;
		},
		"Trace rays through the scene");

	m.def("add_resource_path", &IG::add_resource_path, "Add path to the resource lookup table.");

	m.attr("__version__") = MACRO_STRINGIFY(IGNIS_VERSION);

	py::class_<Vec3>(m, "Vec3", py::buffer_protocol())
		.def_buffer([](Vec3& v) -> py::buffer_info {
			return py::buffer_info(
				&v,										/* Pointer to buffer */
				sizeof(float),							/* Size of one scalar */
				py::format_descriptor<float>::format(), /* Python struct-style format descriptor */
				1,										/* Number of dimensions */
				{ 1, 3 },								/* Buffer dimensions */
				{ sizeof(float) * 3, sizeof(float) });
		})
		.def(py::init([]() { return Vec3{ 0, 0, 0 }; }))
		.def(py::init([](float x, float y, float z) { return Vec3{ x, y, z }; }))
		.def(py::init(&array_vec3))
		.def_readwrite("x", &Vec3::x)
		.def_readwrite("y", &Vec3::y)
		.def_readwrite("z", &Vec3::z);

	py::class_<Settings>(m, "Settings")
		.def_readwrite("eye", &Settings::eye)
		.def_readwrite("dir", &Settings::dir)
		.def_readwrite("up", &Settings::up)
		.def_readwrite("right", &Settings::right)
		.def_readwrite("width", &Settings::width)
		.def_readwrite("height", &Settings::height);

	py::class_<InitialSettings>(m, "InitialSettings")
		.def_readwrite("eye", &InitialSettings::eye)
		.def_readwrite("dir", &InitialSettings::dir)
		.def_readwrite("up", &InitialSettings::up)
		.def_readwrite("fov", &InitialSettings::fov)
		.def_readwrite("width", &InitialSettings::film_width)
		.def_readwrite("height", &InitialSettings::film_height)
		.def("generate", [](const InitialSettings& settings) {
			IG::Camera camera(
				IG::Vector3f(settings.eye.x, settings.eye.y, settings.eye.z),
				IG::Vector3f(settings.dir.x, settings.dir.y, settings.dir.z),
				IG::Vector3f(settings.up.x, settings.up.y, settings.up.z),
				settings.fov,
				(float)settings.film_width / (float)settings.film_height);

			return Settings{
				Vec3{ camera.eye(0), camera.eye(1), camera.eye(2) },
				Vec3{ camera.dir(0), camera.dir(1), camera.dir(2) },
				Vec3{ camera.up(0), camera.up(1), camera.up(2) },
				Vec3{ camera.right(0), camera.right(1), camera.right(2) },
				camera.w,
				camera.h
			};
		});

	py::class_<Ray>(m, "Ray")
		.def(py::init([](py::array_t<float> org, py::array_t<float> dir) { return construct_ray(array_vec3(org), array_vec3(dir), 0, std::numeric_limits<float>::max()); }))
		.def(py::init([](py::array_t<float> org, py::array_t<float> dir, float tmin, float tmax) { return construct_ray(array_vec3(org), array_vec3(dir), tmin, tmax); }))
		.def_readwrite("org", &Ray::org)
		.def_readwrite("dir", &Ray::dir)
		.def_readwrite("tmin", &Ray::tmin)
		.def_readwrite("tmax", &Ray::tmax);
}