#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "Camera.h"
#include "Interface.h"
#include "generated_interface.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

struct Context {
	size_t Width;
	size_t Height;
};

static Context sContext; // Not the best way to handle it

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
		.def(py::init([](py::array_t<float> arr) { 
            auto r = arr.unchecked<1>();
            return Vec3{ r(0), r(1), r(2) }; }))
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
}