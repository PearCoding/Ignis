#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Logger.h"
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
    static std::unique_ptr<Runtime> sInstance;

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
        if (sInstance) {
            IG_LOG(L_ERROR) << "Trying to create multiple runtime instances!" << std::endl;
            return nullptr;
        }

        sInstance = std::make_unique<Runtime>(mOptions);

        if (mSource.empty()) {
            if (!sInstance->loadFromFile(mPath)) {
                sInstance.reset();
                return nullptr;
            }
        } else {
            if (!sInstance->loadFromString(mSource)) {
                sInstance.reset();
                return nullptr;
            }
        }

        return sInstance.get();
    }

    bool exit(const py::object&, const py::object&, const py::object&)
    {
        sInstance.reset();
        flush_io();
        return false; // Propagate all the exceptions
    }
};

std::unique_ptr<Runtime> RuntimeWrap::sInstance;

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
    m.def("flushLog", flush_io);
    m.def("setQuiet", [](bool b) { IG_LOGGER.setQuiet(b); });
    m.def("setVerbose", [](bool b) { IG_LOGGER.setVerbosity(b ? L_DEBUG : L_INFO); });

    py::enum_<CPUArchitecture>(m, "CPUArchitecture")
        .value("ARM", CPUArchitecture::ARM)
        .value("X86", CPUArchitecture::X86)
        .value("Unknown", CPUArchitecture::Unknown);

    py::enum_<GPUVendor>(m, "GPUVendor")
        .value("AMD", GPUVendor::AMD)
        .value("Intel", GPUVendor::Intel)
        .value("Nvidia", GPUVendor::Nvidia)
        .value("Unknown", GPUVendor::Unknown);

    // TODO: Add missing functions, etc
    py::class_<Target>(m, "Target")
        .def(py::init([]() { return Target(); }))
        .def_property_readonly("IsValid", &Target::isValid)
        .def_property_readonly("IsCPU", &Target::isCPU)
        .def_property_readonly("IsGPU", &Target::isGPU)
        .def_property_readonly("GPUVendor", &Target::gpuVendor)
        .def_property_readonly("CPUArchitecture", &Target::cpuArchitecture)
        .def_property("Device", &Target::device, &Target::setDevice)
        .def_property("VectorWidth", &Target::vectorWidth, &Target::setVectorWidth)
        .def_property("ThreadCount", &Target::threadCount, &Target::setThreadCount)
        .def("toString", &Target::toString)
        .def("__str__", &Target::toString)
        .def_static("makeGeneric", &Target::makeGeneric)
        .def_static("makeSingle", &Target::makeSingle)
        .def_static("makeCPU", py::overload_cast<size_t, size_t>(&Target::makeCPU))
        .def_static("makeGPU", &Target::makeGPU)
        .def_static("pickBest", &Target::pickBest)
        .def_static("pickCPU", &Target::pickCPU)
        .def_static("pickGPU", &Target::pickGPU);

    py::class_<RuntimeOptions>(m, "RuntimeOptions")
        .def(py::init([]() { return RuntimeOptions(); }))
        .def_static("makeDefault", &RuntimeOptions::makeDefault, py::arg("trace") = false)
        .def_readwrite("Target", &RuntimeOptions::Target)
        .def_readwrite("DumpShader", &RuntimeOptions::DumpShader)
        .def_readwrite("DumpShaderFull", &RuntimeOptions::DumpShaderFull)
        .def_readwrite("AcquireStats", &RuntimeOptions::AcquireStats)
        .def_readwrite("SPI", &RuntimeOptions::SPI)
        .def_readwrite("OverrideCamera", &RuntimeOptions::OverrideCamera)
        .def_readwrite("OverrideTechnique", &RuntimeOptions::OverrideTechnique);

    py::class_<Ray>(m, "Ray")
        .def(py::init([](const Vector3f& org, const Vector3f& dir) { return Ray{ org, dir, Vector2f(0, FltMax) }; }))
        .def(py::init([](const Vector3f& org, const Vector3f& dir, float tmin, float tmax) { return Ray{ org, dir, Vector2f(tmin, tmax) }; }))
        .def_readwrite("Origin", &Ray::Origin)
        .def_readwrite("Direction", &Ray::Direction)
        .def_readwrite("Range", &Ray::Range);

    py::class_<CameraOrientation>(m, "CameraOrientation")
        .def_readwrite("Eye", &CameraOrientation::Eye)
        .def_readwrite("Dir", &CameraOrientation::Dir)
        .def_readwrite("Up", &CameraOrientation::Up);

    py::class_<Runtime>(m, "Runtime")
        .def("step", &Runtime::step, py::arg("ignoreDenoiser") = false)
        .def("trace", [](Runtime& r, const std::vector<Ray>& rays) {
            r.trace(rays);
            return py::memoryview::from_buffer(
                r.getFramebuffer({}).Data,
                std::vector<size_t>{ rays.size(), 3ul },
                { sizeof(float) * 3, sizeof(float) },
                true);
        })
        .def("reset", &Runtime::reset)
        .def(
            "getFramebuffer", [](const Runtime& r, const std::string& aov) {
                // TODO: Iteration count?
                const size_t width  = r.framebufferWidth();
                const size_t height = r.framebufferHeight();
                return py::memoryview::from_buffer(
                    r.getFramebuffer(aov).Data,                                                         // buffer pointer
                    std::vector<size_t>{ height, width, 3ul },                                          // shape (rows, cols)
                    std::vector<size_t>{ sizeof(float) * width * 3, sizeof(float) * 3, sizeof(float) }, // strides in bytes
                    true);
            },
            py::arg("aov") = "")
        .def("setParameter", py::overload_cast<const std::string&, int>(&Runtime::setParameter))
        .def("setParameter", py::overload_cast<const std::string&, float>(&Runtime::setParameter))
        .def("setParameter", py::overload_cast<const std::string&, const Vector3f&>(&Runtime::setParameter))
        .def("setParameter", py::overload_cast<const std::string&, const Vector4f&>(&Runtime::setParameter))
        .def("setCameraOrientationParameter", &Runtime::setCameraOrientationParameter)
        .def("clearFramebuffer", py::overload_cast<>(&Runtime::clearFramebuffer))
        .def("clearFramebuffer", py::overload_cast<const std::string&>(&Runtime::clearFramebuffer))
        .def_property_readonly("InitialCameraOrientation", &Runtime::initialCameraOrientation)
        .def_property_readonly("IterationCount", &Runtime::currentIterationCount)
        .def_property_readonly("SampleCount", &Runtime::currentSampleCount)
        .def_property_readonly("FramebufferWidth", &Runtime::framebufferWidth)
        .def_property_readonly("FramebufferHeight", &Runtime::framebufferHeight)
        .def_property_readonly("Technique", &Runtime::technique)
        .def_property_readonly("Camera", &Runtime::camera)
        .def_property_readonly("Target", &Runtime::target)
        .def_property_readonly("SPI", &Runtime::samplesPerIteration)
        .def_property_readonly_static("AvailableCameraTypes", &Runtime::getAvailableCameraTypes)
        .def_property_readonly_static("AvailableTechniqueTypes", &Runtime::getAvailableTechniqueTypes);

    py::class_<RuntimeWrap>(m, "RuntimeWrap")
        .def("__enter__", &RuntimeWrap::enter, py::return_value_policy::reference_internal)
        .def("__exit__", &RuntimeWrap::exit);

    m.def("loadFromFile", [](const std::string& path) { return RuntimeWrap(RuntimeOptions::makeDefault(), std::string{}, path); });
    m.def("loadFromFile", [](const std::string& path, const RuntimeOptions& opts) { return RuntimeWrap(opts, std::string{}, path); });
    m.def("loadFromString", [](const std::string& str) { return RuntimeWrap(RuntimeOptions::makeDefault(), str, std::string{}); });
    m.def("loadFromString", [](const std::string& str, const RuntimeOptions& opts) { return RuntimeWrap(opts, str, std::string{}); });
}