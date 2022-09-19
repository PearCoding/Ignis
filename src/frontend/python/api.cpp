#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Runtime.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;
using namespace IG;

static void flush_io()
{
    std::cout.flush();
    std::cerr.flush();
}

class RuntimeWrap {
    std::unique_ptr<Runtime> mInstance;

    RuntimeOptions mOptions;
    std::string mSource;
    std::string mPath;

public:
    RuntimeWrap(const RuntimeOptions& opts, const std::string& source, const std::string& path)
        : mOptions(opts)
        , mSource(source)
        , mPath(path)
    {
        IG_ASSERT(source.empty() ^ path.empty(), "Only source or a path is allowed");
    }

    Runtime* enter()
    {
        mInstance = std::make_unique<Runtime>(mOptions);

        if (mSource.empty()) {
            if (!mInstance->loadFromFile(mPath))
                return nullptr;
        } else {
            if (!mInstance->loadFromString(mSource))
                return nullptr;
        }

        return mInstance.get();
    }

    bool exit(const py::object&, const py::object&, const py::object&)
    {
        mInstance.reset();
        flush_io();
        return true; // TODO: Be more sensitive?
    }
};

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

    // Logger IO stuff
    m.def("flush_log", flush_io);

    py::class_<RuntimeOptions>(m, "RuntimeOptions")
        .def(py::init([]() { return RuntimeOptions(); }))
        .def_readwrite("DesiredTarget", &RuntimeOptions::DesiredTarget)
        .def_readwrite("RecommendCPU", &RuntimeOptions::RecommendCPU)
        .def_readwrite("RecommendGPU", &RuntimeOptions::RecommendGPU)
        .def_readwrite("DumpShader", &RuntimeOptions::DumpShader)
        .def_readwrite("DumpShaderFull", &RuntimeOptions::DumpShaderFull)
        .def_readwrite("AcquireStats", &RuntimeOptions::AcquireStats)
        .def_readwrite("Device", &RuntimeOptions::Device)
        .def_readwrite("SPI", &RuntimeOptions::SPI)
        .def_readwrite("OverrideCamera", &RuntimeOptions::OverrideCamera)
        .def_readwrite("OverrideTechnique", &RuntimeOptions::OverrideTechnique)
        .def_property(
            "ModulePath", [](const RuntimeOptions& opts) { return opts.ModulePath.generic_u8string(); }, [](RuntimeOptions& opts, const std::string& val) { opts.ModulePath = val; });

    py::class_<Ray>(m, "Ray")
        .def(py::init([](const Vector3f& org, const Vector3f& dir) { return Ray{ org, dir, Vector2f(0, 1) }; }))
        .def(py::init([](const Vector3f& org, const Vector3f& dir, float tmin, float tmax) { return Ray{ org, dir, Vector2f(tmin, tmax) }; }))
        .def_readwrite("Origin", &Ray::Origin)
        .def_readwrite("Direction", &Ray::Direction)
        .def_readwrite("Range", &Ray::Range);

    py::enum_<Target>(m, "Target")
        .value("GENERIC", Target::GENERIC)
        .value("SINGLE", Target::SINGLE)
        .value("ASIMD", Target::ASIMD)
        .value("SSE42", Target::SSE42)
        .value("AVX", Target::AVX)
        .value("AVX2", Target::AVX2)
        .value("AVX512", Target::AVX512)
        .value("NVVM", Target::NVVM)
        .value("AMDGPU", Target::AMDGPU);

    py::class_<Runtime>(m, "Runtime")
        .def("step", &Runtime::step)
        .def("trace", [](Runtime& r, const std::vector<Ray>& rays) {
            std::vector<float> data;
            r.trace(rays, data);
            return data;
        })
        .def("reset", &Runtime::reset)
        .def("getFramebuffer", [](const Runtime& r, const std::string& aov) {
            // TODO: Iteration count?
            const size_t width  = r.framebufferWidth();
            const size_t height = r.framebufferHeight();
            return py::memoryview::from_buffer(
                r.getFramebuffer(aov).Data,                                                        // buffer pointer
                std::vector<size_t>{ height, width, 3ul },                                         // shape (rows, cols)
                std::vector<size_t>{ sizeof(float) * width * 3, sizeof(float) * 3, sizeof(float) } // strides in bytes
            );
        })
        .def("setParameter", py::overload_cast<const std::string&, int>(&Runtime::setParameter))
        .def("setParameter", py::overload_cast<const std::string&, float>(&Runtime::setParameter))
        .def("setParameter", py::overload_cast<const std::string&, const Vector3f&>(&Runtime::setParameter))
        .def("setParameter", py::overload_cast<const std::string&, const Vector4f&>(&Runtime::setParameter))
        .def("clearFramebuffer", py::overload_cast<>(&Runtime::clearFramebuffer))
        .def("clearFramebuffer", py::overload_cast<const std::string&>(&Runtime::clearFramebuffer))
        .def_property_readonly("iterationCount", &Runtime::currentIterationCount)
        .def_property_readonly("sampleCount", &Runtime::currentSampleCount)
        .def_property_readonly("framebufferWidth", &Runtime::framebufferWidth)
        .def_property_readonly("framebufferHeight", &Runtime::framebufferHeight)
        .def_property_readonly("technique", &Runtime::technique)
        .def_property_readonly("camera", &Runtime::camera)
        .def_property_readonly("target", &Runtime::target)
        .def_property_readonly("spi", &Runtime::samplesPerIteration);

    py::class_<RuntimeWrap>(m, "RuntimeWrap")
        .def("__enter__", &RuntimeWrap::enter, py::return_value_policy::reference)
        .def("__exit__", &RuntimeWrap::exit);

    m.def("loadFromFile", [](const std::string& path) { return std::make_unique<RuntimeWrap>(RuntimeOptions(), std::string{}, path); });
    m.def("loadFromFile", [](const std::string& path, const RuntimeOptions& opts) { return std::make_unique<RuntimeWrap>(opts, std::string{}, path); });
    m.def("loadFromString", [](const std::string& str) { return std::make_unique<RuntimeWrap>(RuntimeOptions(), str, std::string{}); });
    m.def("loadFromString", [](const std::string& str, const RuntimeOptions& opts) { return std::make_unique<RuntimeWrap>(opts, str, std::string{}); });
}