#include <nanobind/nanobind.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

#include "Image.h"
#include "Logger.h"
#include "Runtime.h"

namespace nb = nanobind;
using namespace nb::literals;

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

    bool exit(const nb::object&, const nb::object&, const nb::object&)
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
void runtime_module(nb::module_& m)
{
    // Logger IO stuff
    m.def("flushLog", flush_io, "Flush internal logs");

    nb::enum_<CPUArchitecture>(m, "CPUArchitecture", "Enum holding supported CPU architectures")
        .value("ARM", CPUArchitecture::ARM)
        .value("X86", CPUArchitecture::X86)
        .value("Unknown", CPUArchitecture::Unknown);

    nb::enum_<GPUArchitecture>(m, "GPUArchitecture", "Enum holding supported GPU architectures")
        .value("AMD", GPUArchitecture::AMD)
        .value("Intel", GPUArchitecture::Intel)
        .value("Nvidia", GPUArchitecture::Nvidia)
        .value("Unknown", GPUArchitecture::Unknown);

    nb::class_<Target>(m, "Target", "Target specification the runtime is using")
        .def(nb::init<>())
        .def_prop_ro("IsValid", &Target::isValid)
        .def_prop_ro("IsCPU", &Target::isCPU)
        .def_prop_ro("IsGPU", &Target::isGPU)
        .def_prop_ro("GPUArchitecture", &Target::gpuArchitecture)
        .def_prop_ro("CPUArchitecture", &Target::cpuArchitecture)
        .def_property("Device", &Target::device, &Target::setDevice)
        .def_property("VectorWidth", &Target::vectorWidth, &Target::setVectorWidth)
        .def_property("ThreadCount", &Target::threadCount, &Target::setThreadCount)
        .def("toString", &Target::toString)
        .def("__str__", &Target::toString)
        .def_static("makeGeneric", &Target::makeGeneric)
        .def_static("makeSingle", &Target::makeSingle)
        .def_static("makeCPU", nb::overload_cast<size_t, size_t>(&Target::makeCPU))
        .def_static("makeGPU", &Target::makeGPU)
        .def_static("pickBest", &Target::pickBest)
        .def_static("pickCPU", &Target::pickCPU)
        .def_static("pickGPU", &Target::pickGPU, nb::arg("device") = 0);

    nb::class_<RuntimeOptions>(m, "RuntimeOptions", "Options to customize runtime behaviour")
        .def(nb::init<>())
        .def_static("makeDefault", &RuntimeOptions::makeDefault, nb::arg("trace") = false)
        .def_readwrite("Target", &RuntimeOptions::Target, "The target device")
        .def_readwrite("DumpShader", &RuntimeOptions::DumpShader, "Set True if most shader should be dumped into the filesystem")
        .def_readwrite("DumpShaderFull", &RuntimeOptions::DumpShaderFull, "Set True if all shader should be dumped into the filesystem")
        .def_readwrite("AcquireStats", &RuntimeOptions::AcquireStats, "Set True if statistical data should be acquired while rendering")
        .def_readwrite("SPI", &RuntimeOptions::SPI, "The requested sample per iteration. Can be 0 to set automatically")
        .def_readwrite("Seed", &RuntimeOptions::Seed, "Seed for the random generators")
        .def_readwrite("OverrideCamera", &RuntimeOptions::OverrideCamera, "Type of camera to use instead of the one used by the scene")
        .def_readwrite("OverrideTechnique", &RuntimeOptions::OverrideTechnique, "Type of technique to use instead of the one used by the scene")
        .def_readwrite("OverrideFilmSize", &RuntimeOptions::OverrideFilmSize, "Type of film size to use instead of the one used by the scene")
        .def_readwrite("EnableTonemapping", &RuntimeOptions::EnableTonemapping, "Set True if any of the two tonemapping functions ``tonemap`` and ``imageinfo`` is to be used")
        .def_readwrite("WarnUnused", &RuntimeOptions::WarnUnused, "Set False if you want to ignore warnings about unused property entries");

    nb::class_<Ray>(m, "Ray", "Single ray traced into the scene")
        .def("__init__", [](const Vector3f& org, const Vector3f& dir) { return Ray{ org, dir, Vector2f(0, FltMax) }; })
        .def("__init__", [](const Vector3f& org, const Vector3f& dir, float tmin, float tmax) { return Ray{ org, dir, Vector2f(tmin, tmax) }; })
        .def_readwrite("Origin", &Ray::Origin, "Origin of the ray")
        .def_readwrite("Direction", &Ray::Direction, "Direction of the ray")
        .def_readwrite("Range", &Ray::Range, "Range (tmin, tmax) of the ray");

    nb::class_<CameraOrientation>(m, "CameraOrientation", "General camera orientation")
        .def_readwrite("Eye", &CameraOrientation::Eye, "Origin of the camera")
        .def_readwrite("Dir", &CameraOrientation::Dir, "Direction the camera is facing")
        .def_readwrite("Up", &CameraOrientation::Up, "Vector defining the up of the camera");

    // TODO: Add option to insert new "texture" as parameter for Python Interchange with proper device handling
    nb::class_<Runtime>(m, "Runtime", "Renderer runtime allowing control of simulation and access to results")
        .def("step", &Runtime::step, nb::arg("ignoreDenoiser") = false)
        .def("trace", [](Runtime& r, const std::vector<Ray>& rays) {
            r.trace(rays);
            size_t shape[] = {rays.size(), 3ul};
            return nb::ndarray<nb::numpy, float, nb::shape<nb::any, 3>>(r.getFramebuffer({}).Data, 2, shape);
        })
        .def("reset", &Runtime::reset)
        .def(
            "getFramebuffer", [](const Runtime& r, const std::string& aov) {
                // TODO: Iteration count?
                // TODO: Add device specific access!
                const size_t width  = r.framebufferWidth();
                const size_t height = r.framebufferHeight();
                size_t shape[] = {width, height, 3ul};
                return nb::ndarray<nb::numpy, float, nb::shape<nb::any, nb::any, 3>>(r.getFramebuffer(aov).Data, 3, shape);
            },
            nb::arg("aov") = "")
        .def("tonemap", [](Runtime& r, nb::ndarray<uint32_t, nb::shape<nb::any, nb::any>, nb::c_contig, nb::device::cpu> output) {
            // TODO: Add device specific access!
            TonemapSettings settings;
            settings.AOV            = "";
            settings.ExposureFactor = 1;
            settings.ExposureOffset = 0;
            settings.Method         = 0;
            settings.Scale          = r.currentIterationCount() > 0 ? 1.0f / r.currentIterationCount() : 1.0f;
            settings.UseGamma       = true;

            const size_t width  = r.framebufferWidth();
            const size_t height = r.framebufferHeight();

            if (output.shape(0) != width || output.shape(1) != height)
                throw std::runtime_error("Incompatible buffer: Buffer has not the correct shape matching the framebuffer size");

            // TODO: Check stride?
            r.tonemap((uint32*)output.data(), settings);
        })
        .def("setParameter", nb::overload_cast<const std::string&, int>(&Runtime::setParameter))
        .def("setParameter", nb::overload_cast<const std::string&, float>(&Runtime::setParameter))
        .def("setParameter", nb::overload_cast<const std::string&, const Vector3f&>(&Runtime::setParameter))
        .def("setParameter", nb::overload_cast<const std::string&, const Vector4f&>(&Runtime::setParameter))
        .def("setCameraOrientationParameter", &Runtime::setCameraOrientationParameter)
        .def("clearFramebuffer", nb::overload_cast<>(&Runtime::clearFramebuffer))
        .def("clearFramebuffer", nb::overload_cast<const std::string&>(&Runtime::clearFramebuffer))
        .def_prop_ro("InitialCameraOrientation", &Runtime::initialCameraOrientation)
        .def_prop_ro("IterationCount", &Runtime::currentIterationCount)
        .def_prop_ro("SampleCount", &Runtime::currentSampleCount)
        .def_prop_ro("FrameCount", &Runtime::currentFrameCount)
        .def("incFrameCount", &Runtime::incFrameCount)
        .def_prop_ro("FramebufferWidth", &Runtime::framebufferWidth)
        .def_prop_ro("FramebufferHeight", &Runtime::framebufferHeight)
        .def_prop_ro("Technique", &Runtime::technique)
        .def_prop_ro("Camera", &Runtime::camera)
        .def_prop_ro("Target", &Runtime::target)
        .def_prop_ro("SPI", &Runtime::samplesPerIteration)
        .def_prop_ro_static("AvailableCameraTypes", &Runtime::getAvailableCameraTypes)
        .def_prop_ro_static("AvailableTechniqueTypes", &Runtime::getAvailableTechniqueTypes);

    nb::class_<RuntimeWrap>(m, "RuntimeWrap", "Wrapper around the runtime used for proper runtime loading and shutdown")
        .def("__enter__", &RuntimeWrap::enter, nb::rv_policy::reference_internal)
        .def("__exit__", &RuntimeWrap::exit)
        .def_prop_ro("instance", &RuntimeWrap::instance, nb::rv_policy::reference_internal)
        .def("shutdown", &RuntimeWrap::shutdown)
        .def("__del__", &RuntimeWrap::shutdown);

    m.def(
         "loadFromFile", [](const std::string& path) { return RuntimeWrap(RuntimeOptions::makeDefault(), std::string{}, path); },
         "Load a scene from file and generate a default runtime")
        .def(
            "loadFromFile", [](const std::string& path, const RuntimeOptions& opts) { return RuntimeWrap(opts, std::string{}, path); },
            "Load a scene from file and generate a runtime with given options")
        .def(
            "loadFromString", [](const std::string& str) { return RuntimeWrap(RuntimeOptions::makeDefault(), str, std::string{}); },
            "Load a scene from a string and generate a default runtime")
        .def(
            "loadFromString", [](const std::string& str, const std::string& dir) { return RuntimeWrap(RuntimeOptions::makeDefault(), str, dir); },
            "Load a scene from a string with directory for external resources and generate a default runtime")
        .def(
            "loadFromString", [](const std::string& str, const RuntimeOptions& opts) { return RuntimeWrap(opts, str, std::string{}); },
            "Load a scene from a string and generate a runtime with given options")
        .def(
            "loadFromString", [](const std::string& str, const std::string& dir, const RuntimeOptions& opts) { return RuntimeWrap(opts, str, dir); },
            "Load a scene from a string with directory for external resources and generate a runtime with given options")
        .def(
            "loadFromScene", [](const std::shared_ptr<Scene>& scene) { return RuntimeWrap(RuntimeOptions::makeDefault(), scene, std::string{}); },
            "Generate a default runtime from an already loaded scene")
        .def(
            "loadFromScene", [](const std::shared_ptr<Scene>& scene, const std::string& dir) { return RuntimeWrap(RuntimeOptions::makeDefault(), scene, dir); },
            "Generate a default runtime from an already loaded scene with directory for external resources")
        .def(
            "loadFromScene", [](const std::shared_ptr<Scene>& scene, const RuntimeOptions& opts) { return RuntimeWrap(opts, scene, std::string{}); },
            "Generate a runtime with given options from an already loaded scene")
        .def(
            "loadFromScene", [](const std::shared_ptr<Scene>& scene, const std::string& dir, const RuntimeOptions& opts) { return RuntimeWrap(opts, scene, dir); },
            "Generate a runtime with given options from an already loaded scene with directory for external resources")
        .def(
            "saveExr", [](const std::string& path, nb::ndarray<float, nb::shape<nb::any, nb::any, 3>, nb::c_contig, nb::device::cpu> b) {
                size_t width  = b.shape(0);
                size_t height = b.shape(1);

                if (width == 0 || height == 0 || b.shape(2) != 3)
                    throw std::runtime_error("Incompatible buffer: Expected valid buffer dimensions");

                return Image::save(path, (const float*)b.data(), width, height, 3);
            },
            "Save an OpenEXR image to the filesystem")
        .def(
            "saveExr", [](const std::string& path, nb::ndarray<float, nb::shape<nb::any, nb::any>, nb::c_contig, nb::device::cpu> b) {
                size_t width  = b.shape(0);
                size_t height = b.shape(1);

                if (width == 0 || height == 0)
                    throw std::runtime_error("Incompatible buffer: Expected valid buffer dimensions");

                return Image::save(path, (const float*)b.data(), width, height, 1);
            },
            "Save an OpenEXR grayscale image to the filesystem");
}