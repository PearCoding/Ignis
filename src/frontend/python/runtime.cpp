#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Image.h"
#include "Logger.h"
#include "Runtime.h"

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
    bool mCreated;
    std::shared_ptr<Scene> mScene;

public:
    RuntimeWrap(const RuntimeOptions& opts, const std::string& source, const std::string& path)
        : mOptions(opts)
        , mSource(source)
        , mPath(path)
        , mCreated(false)
        , mScene()
    {
    }

    RuntimeWrap(const RuntimeOptions& opts, const std::shared_ptr<Scene>& scene, const std::string& path)
        : mOptions(opts)
        , mSource()
        , mPath(path)
        , mCreated(false)
        , mScene(scene)
    {
    }

    Runtime* enter()
    {
        return create();
    }

    bool exit(const py::object&, const py::object&, const py::object&)
    {
        shutdown();
        return false; // Propagate all the exceptions
    }

    Runtime* instance()
    {
        // Only return instance if the class created it
        if (mCreated && sInstance)
            return sInstance.get();

        return create();
    }

    void shutdown()
    {
        sInstance.reset();
        flush_io();
        // Do not reset `mCreated`! The class should create the runtime only once
    }

private:
    Runtime* create()
    {
        if (sInstance) {
            IG_LOG(L_ERROR) << "Trying to create multiple runtime instances!" << std::endl;
            return nullptr;
        }

        sInstance = std::make_unique<Runtime>(mOptions);

        if (mScene) {
            if (!sInstance->loadFromScene(mScene)) {
                sInstance.reset();
                return nullptr;
            }
        } else if (mSource.empty()) {
            if (!sInstance->loadFromFile(mPath)) {
                sInstance.reset();
                return nullptr;
            }
        } else {
            if (!sInstance->loadFromString(mSource, mPath)) {
                sInstance.reset();
                return nullptr;
            }
        }

        mCreated = true;
        return sInstance.get();
    }
};

std::unique_ptr<Runtime> RuntimeWrap::sInstance;
void runtime_module(py::module_& m)
{
    // Logger IO stuff
    m.def("flushLog", flush_io);

    py::enum_<CPUArchitecture>(m, "CPUArchitecture")
        .value("ARM", CPUArchitecture::ARM)
        .value("X86", CPUArchitecture::X86)
        .value("Unknown", CPUArchitecture::Unknown);

    py::enum_<GPUVendor>(m, "GPUVendor")
        .value("AMD", GPUVendor::AMD)
        .value("Intel", GPUVendor::Intel)
        .value("Nvidia", GPUVendor::Nvidia)
        .value("Unknown", GPUVendor::Unknown);

    py::class_<Target>(m, "Target")
        .def(py::init<>())
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
        .def_static("pickGPU", &Target::pickGPU, py::arg("device") = 0);

    py::class_<RuntimeOptions>(m, "RuntimeOptions")
        .def(py::init<>())
        .def_static("makeDefault", &RuntimeOptions::makeDefault, py::arg("trace") = false)
        .def_readwrite("Target", &RuntimeOptions::Target)
        .def_readwrite("DumpShader", &RuntimeOptions::DumpShader)
        .def_readwrite("DumpShaderFull", &RuntimeOptions::DumpShaderFull)
        .def_readwrite("AcquireStats", &RuntimeOptions::AcquireStats)
        .def_readwrite("SPI", &RuntimeOptions::SPI)
        .def_readwrite("OverrideCamera", &RuntimeOptions::OverrideCamera)
        .def_readwrite("OverrideTechnique", &RuntimeOptions::OverrideTechnique)
        .def_readwrite("OverrideFilmSize", &RuntimeOptions::OverrideFilmSize)
        .def_readwrite("EnableTonemapping", &RuntimeOptions::EnableTonemapping);

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
        .def("tonemap", [](Runtime& r, py::buffer output) {
            TonemapSettings settings;
            settings.AOV            = "";
            settings.ExposureFactor = 1;
            settings.ExposureOffset = 0;
            settings.Method         = 0;
            settings.Scale          = r.currentIterationCount() > 0 ? 1.0f / r.currentIterationCount() : 1.0f;
            settings.UseGamma       = true;

            const size_t width  = r.framebufferWidth();
            const size_t height = r.framebufferHeight();

            py::buffer_info info = output.request(true);

            /* Some basic validation checks ... */
            if (info.format != py::format_descriptor<uint8>::format())
                throw std::runtime_error("Incompatible buffer: Expected a byte array!");

            if (info.ndim != 1 && info.ndim != 2 && info.ndim != 3)
                throw std::runtime_error("Incompatible buffer: Expected one, two or three dimensional buffer");

            py::ssize_t linear_size = 1;
            for (py::ssize_t i = 0; i < info.ndim; ++i)
                linear_size *= info.shape[i];

            if (linear_size != static_cast<py::ssize_t>(width * height * 4))
                throw std::runtime_error("Incompatible buffer: Buffer has not the correct size");

            // TODO: Check stride?
            r.tonemap((uint32*)info.ptr, settings);
        })
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
        .def("__exit__", &RuntimeWrap::exit)
        .def_property_readonly("instance", &RuntimeWrap::instance, py::return_value_policy::reference_internal)
        .def("shutdown", &RuntimeWrap::shutdown)
        .def("__del__", &RuntimeWrap::shutdown);

    m.def("loadFromFile", [](const std::string& path) { return RuntimeWrap(RuntimeOptions::makeDefault(), std::string{}, path); })
        .def("loadFromFile", [](const std::string& path, const RuntimeOptions& opts) { return RuntimeWrap(opts, std::string{}, path); })
        .def("loadFromString", [](const std::string& str) { return RuntimeWrap(RuntimeOptions::makeDefault(), str, std::string{}); })
        .def("loadFromString", [](const std::string& str, const std::string& dir) { return RuntimeWrap(RuntimeOptions::makeDefault(), str, dir); })
        .def("loadFromString", [](const std::string& str, const RuntimeOptions& opts) { return RuntimeWrap(opts, str, std::string{}); })
        .def("loadFromString", [](const std::string& str, const std::string& dir, const RuntimeOptions& opts) { return RuntimeWrap(opts, str, dir); })
        .def("loadFromScene", [](const std::shared_ptr<Scene>& scene) { return RuntimeWrap(RuntimeOptions::makeDefault(), scene, std::string{}); })
        .def("loadFromScene", [](const std::shared_ptr<Scene>& scene, const std::string& dir) { return RuntimeWrap(RuntimeOptions::makeDefault(), scene, dir); })
        .def("loadFromScene", [](const std::shared_ptr<Scene>& scene, const RuntimeOptions& opts) { return RuntimeWrap(opts, scene, std::string{}); })
        .def("loadFromScene", [](const std::shared_ptr<Scene>& scene, const std::string& dir, const RuntimeOptions& opts) { return RuntimeWrap(opts, scene, dir); })
        .def("saveExr", [](const std::string& path, py::buffer b) {
            py::buffer_info info = b.request();

            /* Some basic validation checks ... */
            if (info.format != py::format_descriptor<float>::format())
                throw std::runtime_error("Incompatible buffer: Expected a float array!");

            if (info.ndim != 2 && info.ndim != 3)
                throw std::runtime_error("Incompatible buffer: Expected two dimensional or three dimensional buffer");

            pybind11::ssize_t width    = info.shape[0];
            pybind11::ssize_t height   = info.shape[1];
            pybind11::ssize_t channels = info.ndim == 3 ? info.shape[2] : 1;

            if (width <= 0 || height <= 0)
                throw std::runtime_error("Incompatible buffer: Expected valid buffer dimensions");

            if (channels != 1 && channels != 3 && channels != 4)
                throw std::runtime_error("Incompatible buffer: Only 1, 3 or 4 channels supported");

            return Image::save(path, (const float*)info.ptr, width, height, channels);
        });
}