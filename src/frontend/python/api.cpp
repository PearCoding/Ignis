#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Camera.h"
#include "Runtime.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;
using namespace IG;

PYBIND11_MODULE(pyignis, m)
{
	m.doc() = R"pbdoc(
        Ignis python scene plugin
        -----------------------
        .. currentmodule:: pyignis
        .. autosummary::
           :toctree: _generate
    )pbdoc";

	m.attr("__version__") = MACRO_STRINGIFY(IGNIS_VERSION);

	py::class_<RuntimeOptions>(m, "RuntimeOptions")
		.def_readwrite("DesiredTarget", &RuntimeOptions::DesiredTarget)
		.def_readwrite("Device", &RuntimeOptions::Device);

	py::class_<RuntimeRenderSettings>(m, "RuntimeRenderSettings")
		.def_readwrite("FilmWidth", &RuntimeRenderSettings::FilmWidth)
		.def_readwrite("FilmHeight", &RuntimeRenderSettings::FilmHeight);

	py::class_<Camera>(m, "Camera")
		.def_readwrite("Eye", &Camera::eye)
		.def_readwrite("Direction", &Camera::dir)
		.def_readwrite("Right", &Camera::right)
		.def_readwrite("Up", &Camera::up)
		.def_readwrite("SensorWidth", &Camera::w)
		.def_readwrite("SensorHeight", &Camera::h);

	py::class_<Ray>(m, "Ray")
		.def(py::init([](const Vector3f& org, const Vector3f& dir) { return Ray{ org, dir, Vector2f(0, 1) }; }))
		.def(py::init([](const Vector3f& org, const Vector3f& dir, float tmin, float tmax) { return Ray{ org, dir, Vector2f(tmin, tmax) }; }))
		.def_readwrite("Origin", &Ray::Origin)
		.def_readwrite("Direction", &Ray::Direction)
		.def_readwrite("Range", &Ray::Range);

	py::class_<Runtime>(m, "Runtime")
		.def(py::init([](const std::string& path, const RuntimeOptions& opts) { return std::make_unique<Runtime>(path, opts); }))
		.def("setup", &Runtime::setup)
		.def("step", &Runtime::step)
		.def("trace", [](Runtime& r, const std::vector<Ray>& rays) {
			std::vector<float> data;
			r.trace(rays, data);
			return data;
		})
		.def("getFramebuffer", [](const Runtime& r, uint32 aov) {
			const size_t width	= r.loadedRenderSettings().FilmWidth;
			const size_t height = r.loadedRenderSettings().FilmHeight;
			return py::memoryview::from_buffer(
				r.getFramebuffer(aov),											// buffer pointer
				{ height, width, 3ul },											// shape (rows, cols)
				{ sizeof(float) * width * 3, sizeof(float) * 3, sizeof(float) } // strides in bytes
			);
		})
		.def("clearFramebuffer", &Runtime::clearFramebuffer)
		.def_property_readonly("iterationCount", &Runtime::iterationCount)
		.def_property_readonly("loadedRenderSettings", &Runtime::loadedRenderSettings);
}