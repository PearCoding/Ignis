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
        Ignis python plugin
        -----------------------
        .. currentmodule:: pyignis
        .. autosummary::
           :toctree: _generate
    )pbdoc";

    m.attr("__version__") = MACRO_STRINGIFY(IGNIS_VERSION);

    py::class_<RuntimeOptions>(m, "RuntimeOptions")
        .def(py::init([]() { return RuntimeOptions(); }))
        .def_readwrite("DesiredTarget", &RuntimeOptions::DesiredTarget)
        .def_readwrite("Device", &RuntimeOptions::Device)
        .def_readwrite("OverrideCamera", &RuntimeOptions::OverrideCamera)
        .def_readwrite("OverrideTechnique", &RuntimeOptions::OverrideTechnique);

    py::class_<RuntimeRenderSettings>(m, "RuntimeRenderSettings")
        .def(py::init([]() { return RuntimeRenderSettings(); }))
        .def_readwrite("FilmWidth", &RuntimeRenderSettings::FilmWidth)
        .def_readwrite("FilmHeight", &RuntimeRenderSettings::FilmHeight);

    py::class_<Camera>(m, "Camera")
        .def(py::init([](const Vector3f& e, const Vector3f& d, const Vector3f& u, float fov, float ratio, float tmin, float tmax) { return Camera(e, d, u, fov, ratio, tmin, tmax); }))
        .def_readwrite("Eye", &Camera::Eye)
        .def_readwrite("Direction", &Camera::Direction)
        .def_readwrite("Right", &Camera::Right)
        .def_readwrite("Up", &Camera::Up)
        .def_readwrite("SensorWidth", &Camera::SensorWidth)
        .def_readwrite("SensorHeight", &Camera::SensorHeight);

    py::class_<Ray>(m, "Ray")
        .def(py::init([](const Vector3f& org, const Vector3f& dir) { return Ray{ org, dir, Vector2f(0, 1) }; }))
        .def(py::init([](const Vector3f& org, const Vector3f& dir, float tmin, float tmax) { return Ray{ org, dir, Vector2f(tmin, tmax) }; }))
        .def_readwrite("Origin", &Ray::Origin)
        .def_readwrite("Direction", &Ray::Direction)
        .def_readwrite("Range", &Ray::Range);

    py::enum_<Target>(m, "Target")
        .value("GENERIC", Target::GENERIC)
        .value("ASIMD", Target::ASIMD)
        .value("SSE42", Target::SSE42)
        .value("AVX", Target::AVX)
        .value("AVX2", Target::AVX2)
        .value("AVX512", Target::AVX512)
        .value("NVVM", Target::NVVM)
        .value("AMDGPU", Target::AMDGPU);

    py::class_<Runtime>(m, "Runtime")
        .def(py::init([](const std::string& path) { return std::make_unique<Runtime>(path, RuntimeOptions()); }))
        .def(py::init([](const std::string& path, const RuntimeOptions& opts) { return std::make_unique<Runtime>(path, opts); }))
        .def("setup", &Runtime::setup)
        .def("step", &Runtime::step)
        .def("trace", [](Runtime& r, const std::vector<Ray>& rays) {
            std::vector<float> data;
            r.trace(rays, data);
            return data;
        })
        .def("reset", &Runtime::reset)
        .def("getFramebuffer", [](const Runtime& r, uint32 aov) {
            const size_t width  = r.loadedRenderSettings().FilmWidth;
            const size_t height = r.loadedRenderSettings().FilmHeight;
            return py::memoryview::from_buffer(
                r.getFramebuffer(aov),                                          // buffer pointer
                { height, width, 3ul },                                         // shape (rows, cols)
                { sizeof(float) * width * 3, sizeof(float) * 3, sizeof(float) } // strides in bytes
            );
        })
        .def("clearFramebuffer", &Runtime::clearFramebuffer)
        .def_property_readonly("iterationCount", &Runtime::currentIterationCount)
        .def_property_readonly("iterationCountForFramebuffer", &Runtime::currentIterationCountForFramebuffer)
        .def_property_readonly("loadedRenderSettings", &Runtime::loadedRenderSettings);
}