#include "Runtime.h"
#include "Image.h"
#include "Logger.h"

IG_BEGIN_IGNORE_WARNINGS
#include <nanobind/eigen/dense.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/vector.h>
IG_END_IGNORE_WARNINGS

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
    Path mPath;
    bool mCreated;
    const Scene* mScene;

public:
    RuntimeWrap(const RuntimeOptions& opts, const std::string& source, const Path& path)
        : mOptions(opts)
        , mSource(source)
        , mPath(path)
        , mCreated(false)
        , mScene(nullptr)
    {
    }

    RuntimeWrap(const RuntimeOptions& opts, const Scene* scene, const Path& path)
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
        .value("AMD", GPUArchitecture::AMD_HSA)
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
        .def_prop_ro("Architecture", &Target::architecture)
        .def_prop_rw("Device", &Target::device, &Target::setDevice)
        .def_prop_rw("VectorWidth", &Target::vectorWidth, &Target::setVectorWidth)
        .def_prop_rw("ThreadCount", &Target::threadCount, &Target::setThreadCount)
        .def("toString", [](const Target& t) { return t.toString(); })
        .def("__str__", [](const Target& t) { return t.toString(); })
        .def_static("makeGeneric", &Target::makeGeneric)
        .def_static("makeSingle", &Target::makeSingle)
        .def_static("makeCPU", nb::overload_cast<size_t, size_t>(&Target::makeCPU))
        .def_static("makeGPU", &Target::makeGPU)
        .def_static("pickBest", &Target::pickBest)
        .def_static("pickCPU", &Target::pickCPU)
        .def_static("pickGPU", &Target::pickGPU, nb::arg("device") = 0);

    nb::class_<DenoiserSettings>(m, "DenoiserSettings", "Settings for the denoiser")
        .def(nb::init<>())
        .def_rw("Enabled", &DenoiserSettings::Enabled, "Enable or disable the denoiser")
        .def_rw("HighQuality", &DenoiserSettings::HighQuality, "Set True if denoiser should be high quality or interactive")
        .def_rw("Prefilter", &DenoiserSettings::Prefilter, "Set True if normal and albedo layer should be prefiltered");

    auto opts = nb::class_<RuntimeOptions>(m, "RuntimeOptions", "Options to customize runtime behaviour")
        .def(nb::init<>())
        .def_static("makeDefault", &RuntimeOptions::makeDefault, nb::arg("trace") = false)
        .def_rw("Target", &RuntimeOptions::Target, "The target device")
        .def_rw("DumpShader", &RuntimeOptions::DumpShader, "Set True if most shader should be dumped into the filesystem")
        .def_rw("DumpShaderFull", &RuntimeOptions::DumpShaderFull, "Set True if all shader should be dumped into the filesystem")
        .def_rw("DumpRegistry", &RuntimeOptions::DumpRegistry, "Dump the registry into the standard io")
        .def_rw("DumpRegistryFull", &RuntimeOptions::DumpRegistryFull, "Dump the complete registry into the standard io")
        .def_rw("DebugTrace", &RuntimeOptions::DebugTrace, "Dump trace information into the logger. Huge negative performance impact")
        .def_rw("AcquireStats", &RuntimeOptions::AcquireStats, "Set True if statistical data should be acquired while rendering")
        .def_rw("SPI", &RuntimeOptions::SPI, "The requested sample per iteration. Can be 0 to set automatically")
        .def_rw("Seed", &RuntimeOptions::Seed, "Seed for the random generators")
        .def_rw("OverrideCamera", &RuntimeOptions::OverrideCamera, "Type of camera to use instead of the one used by the scene")
        .def_rw("OverrideTechnique", &RuntimeOptions::OverrideTechnique, "Type of technique to use instead of the one used by the scene")
        .def_rw("OverrideFilmSize", &RuntimeOptions::OverrideFilmSize, "Type of film size to use instead of the one used by the scene")
        .def_rw("EnableTonemapping", &RuntimeOptions::EnableTonemapping, "Set True if any of the two tonemapping functions ``tonemap`` and ``imageinfo`` is to be used")
        .def_rw("Denoiser", &RuntimeOptions::Denoiser, "Settings for the denoiser")
        .def_rw("WarnUnused", &RuntimeOptions::WarnUnused, "Set False if you want to ignore warnings about unused property entries")
        .def_rw("DisableStandardAOVs", &RuntimeOptions::DisableStandardAOVs, "Disable standard normal and albedo aovs")
        .def_rw("ShaderOptimizationLevel", &RuntimeOptions::ShaderOptimizationLevel, "Level of optimization for shaders")
        .def_rw("ShaderCompileThreads", &RuntimeOptions::ShaderCompileThreads, "Number of threads to use for compiling shaders")
        .def_rw("Specialization", &RuntimeOptions::Specialization)
        .def_rw("EnableCache", &RuntimeOptions::EnableCache, "Enable cache")
        .def_rw("CacheDir", &RuntimeOptions::CacheDir, "The explicit directory for the runtime cache")
        .def_rw("ScriptDir", &RuntimeOptions::ScriptDir, "Path to a new script directory, replacing the internal standard library")
        .def_rw("AddExtraEnvLight", &RuntimeOptions::AddExtraEnvLight, "Option to add a constant environment light (just to see something)");

    nb::enum_<RuntimeOptions::SpecializationMode>(opts, "SpecializationMode", "Enum holding shader specialization modes")
        .value("Default", RuntimeOptions::SpecializationMode::Default)
        .value("Force", RuntimeOptions::SpecializationMode::Force)
        .value("Disable", RuntimeOptions::SpecializationMode::Disable);

    nb::class_<Ray>(m, "Ray", "Single ray traced into the scene")
        .def_static("__init__", [](Ray* ray, const Vector3f& org, const Vector3f& dir) { new (ray) Ray{ org, dir, Vector2f(0, FltMax) }; })
        .def_static("__init__", [](Ray* ray, const Vector3f& org, const Vector3f& dir, float tmin, float tmax) { new (ray) Ray{ org, dir, Vector2f(tmin, tmax) }; })
        .def_rw("Origin", &Ray::Origin, "Origin of the ray")
        .def_rw("Direction", &Ray::Direction, "Direction of the ray")
        .def_rw("Range", &Ray::Range, "Range (tmin, tmax) of the ray");

    nb::class_<CameraOrientation>(m, "CameraOrientation", "General camera orientation")
        .def_rw("Eye", &CameraOrientation::Eye, "Origin of the camera")
        .def_rw("Dir", &CameraOrientation::Dir, "Direction the camera is facing")
        .def_rw("Up", &CameraOrientation::Up, "Vector defining the up of the camera");

    nb::class_<BoundingBox>(m, "BoundingBox", "BoundingBox")
        .def(nb::init<>())
        .def(nb::init<Vector3f>())
        .def(nb::init<Vector3f, Vector3f>())
        .def_rw("Min", &BoundingBox::min)
        .def_rw("Max", &BoundingBox::max)
        .def_prop_ro("Diameter", &BoundingBox::diameter)
        .def_prop_ro("Center", &BoundingBox::center)
        .def_prop_ro("HalfArea", &BoundingBox::halfArea)
        .def_prop_ro("Volume", &BoundingBox::volume)
        .def_prop_ro("IsEmpty", &BoundingBox::isEmpty)
        .def("inflate", &BoundingBox::inflate)
        .def("overlap", &BoundingBox::overlap, nb::rv_policy::reference_internal)
        .def("extend", nb::overload_cast<const BoundingBox&>(&BoundingBox::extend), nb::rv_policy::reference_internal)
        .def("extend", nb::overload_cast<const Vector3f&>(&BoundingBox::extend), nb::rv_policy::reference_internal)
        .def("isInside", &BoundingBox::isInside)
        .def("isOverlapping", &BoundingBox::isOverlapping)
        .def_static("MakeEmpty", &BoundingBox::Empty)
        .def_static("MakeFull", &BoundingBox::Full);

    // TODO: Add option to insert new "texture" as parameter for Python Interchange with proper device handling
    nb::class_<Runtime>(m, "Runtime", "Renderer runtime allowing control of simulation and access to results")
        .def("step", &Runtime::step, nb::arg("ignoreDenoiser") = false)
        .def("trace", [](Runtime& r, const std::vector<Ray>& rays) {
            r.trace(rays);
            size_t shape[] = { rays.size(), 3ul };
            return nb::ndarray<nb::numpy, float, nb::shape<-1, 3>>(r.getFramebufferForHost(std::string{}).Data, 2, shape, nb::handle());
        })
        .def("reset", &Runtime::reset, "Reset internal counters etc. This should be used if data (like camera orientation) has changed. Frame counter will NOT be reset")
        .def(
            "getFramebufferForHost", [](const Runtime& r, const std::string& aov) {
                const size_t width  = r.framebufferWidth();
                const size_t height = r.framebufferHeight();
                size_t shape[]      = { height, width, 3ul };
                return nb::ndarray<nb::numpy, float, nb::shape<-1, -1, 3>, nb::c_contig, nb::device::cpu>(r.getFramebufferForHost(aov).Data, 3, shape, nb::handle()); }, nb::arg("aov") = "")
        .def(
            "getFramebufferForDevice", [](const Runtime& r, const std::string& aov) {
                const size_t width  = r.framebufferWidth();
                const size_t height = r.framebufferHeight();
                size_t shape[]      = { height, width, 3ul };

                const Target target = r.target();

                int32_t deviceType = nb::device::cpu::value;
                int32_t deviceId   = 0;
                if (target.isGPU()) {
                    deviceId = (int32_t)target.device();
                    switch (target.gpuArchitecture()) {
                    case GPUArchitecture::AMD_HSA:
                        deviceType = nb::device::rocm::value;
                        break;
                    case GPUArchitecture::Intel:
                        deviceType = nb::device::oneapi::value; // FIXME: Do not think this is the correct approach
                        break;
                    default:
                    case GPUArchitecture::Nvidia:
                        deviceType = nb::device::cuda::value;
                        break;
                    }
                }

                return nb::ndarray<nb::numpy, float, nb::shape<-1, -1, 3>>(r.getFramebufferForDevice(aov).Data, 3, shape, nb::handle(), nullptr, nb::dtype<float>(), deviceType, deviceId); }, nb::arg("aov") = "")
        .def("tonemap", [](Runtime& r, nb::ndarray<uint32_t, nb::ndim<2>, nb::c_contig, nb::device::cpu> output) {
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

            if (output.shape(1) != width || output.shape(0) != height)
                throw nb::buffer_error("Incompatible buffer: Buffer has not the correct shape matching the framebuffer size");

            // TODO: Check stride?
            r.tonemap((uint32*)output.data(), settings); })
        .def("setParameter", nb::overload_cast<const std::string&, int>(&Runtime::setParameter))
        .def("setParameter", nb::overload_cast<const std::string&, float>(&Runtime::setParameter))
        .def("setParameter", nb::overload_cast<const std::string&, const Vector3f&>(&Runtime::setParameter))
        .def("setParameter", nb::overload_cast<const std::string&, const Vector4f&>(&Runtime::setParameter))
        .def("setParameter", nb::overload_cast<const std::string&, const std::string&>(&Runtime::setParameter))
        .def_prop_ro("IntParameters", [](const Runtime& r) { return r.parameters().IntParameters; })
        .def_prop_ro("FloatParameters", [](const Runtime& r) { return r.parameters().FloatParameters; })
        .def_prop_ro("VectorParameters", [](const Runtime& r) { return r.parameters().VectorParameters; })
        .def_prop_ro("ColorParameters", [](const Runtime& r) { return r.parameters().ColorParameters; })
        .def_prop_ro("StringParameters", [](const Runtime& r) { return r.parameters().StringParameters; })
        .def("setCameraOrientation", &Runtime::setCameraOrientation)
        .def("clearFramebuffer", nb::overload_cast<>(&Runtime::clearFramebuffer))
        .def("clearFramebuffer", nb::overload_cast<const std::string&>(&Runtime::clearFramebuffer))
        .def("saveFramebuffer", &Runtime::saveFramebuffer)
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
        .def_prop_ro("Seed", &Runtime::seed)
        .def_prop_ro("SPI", &Runtime::samplesPerIteration)
        .def_prop_ro("RenderStartTime", [](const Runtime& r) { return std::chrono::duration_cast<std::chrono::seconds>(r.renderStartTime().time_since_epoch()).count(); })
        .def_prop_ro("SceneBoundingBox", &Runtime::sceneBoundingBox)
        .def_prop_ro("AOVs", &Runtime::aovs)
        .def_prop_ro_static("AvailableCameraTypes", &Runtime::getAvailableCameraTypes)
        .def_prop_ro_static("AvailableTechniqueTypes", &Runtime::getAvailableTechniqueTypes);

    nb::class_<RuntimeWrap>(m, "RuntimeWrap", "Wrapper around the runtime used for proper runtime loading and shutdown")
        .def("__enter__", &RuntimeWrap::enter, nb::rv_policy::reference_internal)
        .def("__exit__", &RuntimeWrap::exit, "type"_a.none(), "value"_a.none(), "traceback"_a.none())
        .def_prop_ro("instance", &RuntimeWrap::instance, nb::rv_policy::reference_internal)
        .def("shutdown", &RuntimeWrap::shutdown)
        .def("__del__", &RuntimeWrap::shutdown);

    m.def(
         "loadFromFile", [](const Path& path) { return RuntimeWrap(RuntimeOptions::makeDefault(), std::string{}, path); },
         "Load a scene from file and generate a default runtime")
        .def(
            "loadFromFile", [](const Path& path, const RuntimeOptions& opts) { return RuntimeWrap(opts, std::string{}, path); },
            "Load a scene from file and generate a runtime with given options")
        .def(
            "loadFromString", [](const std::string& str) { return RuntimeWrap(RuntimeOptions::makeDefault(), str, std::string{}); },
            "Load a scene from a string and generate a default runtime")
        .def(
            "loadFromString", [](const std::string& str, const Path& dir) { return RuntimeWrap(RuntimeOptions::makeDefault(), str, dir); },
            "Load a scene from a string with directory for external resources and generate a default runtime")
        .def(
            "loadFromString", [](const std::string& str, const RuntimeOptions& opts) { return RuntimeWrap(opts, str, std::string{}); },
            "Load a scene from a string and generate a runtime with given options")
        .def(
            "loadFromString", [](const std::string& str, const Path& dir, const RuntimeOptions& opts) { return RuntimeWrap(opts, str, dir); },
            "Load a scene from a string with directory for external resources and generate a runtime with given options")
        .def(
            "loadFromScene", [](const Scene* scene) { return RuntimeWrap(RuntimeOptions::makeDefault(), scene, std::string{}); },
            "Generate a default runtime from an already loaded scene")
        .def(
            "loadFromScene", [](const Scene* scene, const Path& dir) { return RuntimeWrap(RuntimeOptions::makeDefault(), scene, dir); },
            "Generate a default runtime from an already loaded scene with directory for external resources")
        .def(
            "loadFromScene", [](const Scene* scene, const RuntimeOptions& opts) { return RuntimeWrap(opts, scene, std::string{}); },
            "Generate a runtime with given options from an already loaded scene")
        .def(
            "loadFromScene", [](const Scene* scene, const Path& dir, const RuntimeOptions& opts) { return RuntimeWrap(opts, scene, dir); },
            "Generate a runtime with given options from an already loaded scene with directory for external resources")
        .def(
            "saveExr", [](const Path& path, nb::ndarray<float, nb::shape<-1, -1, 3>, nb::c_contig, nb::device::cpu> b) {
                size_t width  = b.shape(1);
                size_t height = b.shape(0);

                if (width == 0 || height == 0 || b.shape(2) != 3)
                    throw nb::buffer_error("Incompatible buffer: Expected valid buffer dimensions");

                return Image::save(path, (const float*)b.data(), width, height, 3);
            },
            "Save an OpenEXR image to the filesystem")
        .def(
            "saveExr", [](const Path& path, nb::ndarray<float, nb::ndim<2>, nb::c_contig, nb::device::cpu> b) {
                size_t width  = b.shape(1);
                size_t height = b.shape(0);

                if (width == 0 || height == 0)
                    throw nb::buffer_error("Incompatible buffer: Expected valid buffer dimensions");

                return Image::save(path, (const float*)b.data(), width, height, 1); }, "Save an OpenEXR grayscale image to the filesystem");
}