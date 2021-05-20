#include "Camera.h"

#ifdef WITH_UI
#include "UI.h"
#endif

#include "Logger.h"
#include "Runtime.h"
#include "driver/Configuration.h"

#include <optional>

constexpr int SPP = 4; // Render SPP is always 4!

using namespace IG;

static inline void check_arg(int argc, char** argv, int arg, int n)
{
	if (arg + n >= argc)
		IG_LOG(L_ERROR) << "Option '" << argv[arg] << "' expects " << n << " arguments, got " << (argc - arg) << std::endl;
}

static inline void usage()
{
	std::cout << "Usage: ignis file [options]\n"
			  << "Available options:\n"
			  << "   -h      --help                 Shows this message\n"
			  << "           --width     pixels     Sets the viewport horizontal dimension (in pixels)\n"
			  << "           --height    pixels     Sets the viewport vertical dimension (in pixels)\n"
			  << "           --eye       x y z      Sets the position of the camera\n"
			  << "           --dir       x y z      Sets the direction vector of the camera\n"
			  << "           --up        x y z      Sets the up vector of the camera\n"
			  << "           --fov       degrees    Sets the horizontal field of view (in degrees)\n"
			  << "           --range     tmin tmax  Sets near and far clip range in world units\n"
			  << "           --camera    cam_type   Override camera type\n"
			  << "           --technique tech_type  Override technique/integrator type\n"
			  << "   -t      --target    target     Sets the target platform (default: autodetect CPU)\n"
			  << "   -d      --device    device     Sets the device to use on the selected platform (default: 0)\n"
			  << "           --cpu                  Use autodetected CPU target\n"
			  << "           --gpu                  Use autodetected GPU target\n"
			  << "           --spp       spp        Enables benchmarking mode and sets the number of iterations based on the given spp\n"
			  << "           --bench     iterations Enables benchmarking mode and sets the number of iterations\n"
			  << "   -o      --output    image.exr  Writes the output image to a file\n"
			  << "Available targets:\n"
			  << "    generic, sse42, avx, avx2, avx512, asimd,\n"
			  << "    nvvm = nvvm-streaming, nvvm-megakernel,\n"
			  << "    amdgpu = amdgpu-streaming, amdgpu-megakernel\n"
			  << "Available cameras:\n"
			  << "    perspective, orthogonal, fishlens\n"
			  << "Available techniques:\n"
			  << "    path, debug, ao" << std::endl;
}

int main(int argc, char** argv)
{
	std::string in_file;
	std::string out_file;
	size_t bench_iter = 0;
	std::optional<int> a_film_width;
	std::optional<int> a_film_height;
	std::optional<Vector3f> eye;
	std::optional<Vector3f> dir;
	std::optional<Vector3f> up;
	std::optional<float> fov;
	std::optional<Vector2f> trange;
	Target target = Target::INVALID;
	int device	  = 0;
	std::string overrideTechnique;
	std::string overrideCamera;

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "--width")) {
				check_arg(argc, argv, i, 1);
				a_film_width = strtoul(argv[++i], nullptr, 10);
			} else if (!strcmp(argv[i], "--height")) {
				check_arg(argc, argv, i, 1);
				a_film_height = strtoul(argv[++i], nullptr, 10);
			} else if (!strcmp(argv[i], "--eye")) {
				check_arg(argc, argv, i, 3);
				eye = { Vector3f(strtof(argv[++i], nullptr), strtof(argv[++i], nullptr), strtof(argv[++i], nullptr)) };
			} else if (!strcmp(argv[i], "--dir")) {
				check_arg(argc, argv, i, 3);
				dir = { Vector3f(strtof(argv[++i], nullptr), strtof(argv[++i], nullptr), strtof(argv[++i], nullptr)) };
			} else if (!strcmp(argv[i], "--up")) {
				check_arg(argc, argv, i, 3);
				up = { Vector3f(strtof(argv[++i], nullptr), strtof(argv[++i], nullptr), strtof(argv[++i], nullptr)) };
			} else if (!strcmp(argv[i], "--range")) {
				check_arg(argc, argv, i, 2);
				trange = { Vector2f(strtof(argv[++i], nullptr), strtof(argv[++i], nullptr)) };
			} else if (!strcmp(argv[i], "--fov")) {
				check_arg(argc, argv, i, 1);
				fov = strtof(argv[++i], nullptr);
			} else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--target")) {
				check_arg(argc, argv, i++, 1);
				if (!strcmp(argv[i], "sse42"))
					target = Target::SSE42;
				else if (!strcmp(argv[i], "avx"))
					target = Target::AVX;
				else if (!strcmp(argv[i], "avx2"))
					target = Target::AVX2;
				else if (!strcmp(argv[i], "avx512"))
					target = Target::AVX512;
				else if (!strcmp(argv[i], "asimd"))
					target = Target::ASIMD;
				else if (!strcmp(argv[i], "nvvm") || !strcmp(argv[i], "nvvm-streaming"))
					target = Target::NVVM_STREAMING;
				else if (!strcmp(argv[i], "nvvm-megakernel"))
					target = Target::NVVM_MEGAKERNEL;
				else if (!strcmp(argv[i], "amdgpu") || !strcmp(argv[i], "amdgpu-streaming"))
					target = Target::AMDGPU_STREAMING;
				else if (!strcmp(argv[i], "amdgpu-megakernel"))
					target = Target::AMDGPU_MEGAKERNEL;
				else if (!strcmp(argv[i], "generic"))
					target = Target::GENERIC;
				else {
					IG_LOG(L_ERROR) << "Unknown target '" << argv[i] << "'. Aborting." << std::endl;
					return EXIT_FAILURE;
				}
			} else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--device")) {
				check_arg(argc, argv, i++, 1);
				device = strtoul(argv[i], NULL, 10);
			} else if (!strcmp(argv[i], "--cpu")) {
				target = getRecommendedCPUTarget();
			} else if (!strcmp(argv[i], "--gpu")) {
				target = Target::NVVM_STREAMING; // TODO: Select based on environment
			} else if (!strcmp(argv[i], "--spp")) {
				check_arg(argc, argv, i, 1);
				bench_iter = (size_t)std::ceil(strtoul(argv[++i], nullptr, 10) / (float)SPP);
			} else if (!strcmp(argv[i], "--bench")) {
				check_arg(argc, argv, i, 1);
				bench_iter = strtoul(argv[++i], nullptr, 10);
			} else if (!strcmp(argv[i], "-o")) {
				check_arg(argc, argv, i, 1);
				out_file = argv[++i];
			} else if (!strcmp(argv[i], "--camera")) {
				check_arg(argc, argv, i, 1);
				overrideCamera = argv[++i];
			} else if (!strcmp(argv[i], "--technique")) {
				check_arg(argc, argv, i, 1);
				overrideTechnique = argv[++i];
			} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
				usage();
				return EXIT_SUCCESS;
			} else {
				IG_LOG(L_ERROR) << "Unknown option '" << argv[i] << "'" << std::endl;
				return EXIT_FAILURE;
			}
		} else {
			if (in_file.empty()) {
				in_file = argv[i];
			} else {
				IG_LOG(L_ERROR) << "Unexpected argument '" << argv[i] << "'" << std::endl;
				return EXIT_FAILURE;
			}
		}
	}

	if (target == Target::INVALID)
		target = getRecommendedCPUTarget();

	if (in_file == "") {
		IG_LOG(L_ERROR) << "No input file given" << std::endl;
		return EXIT_FAILURE;
	}

#ifndef WITH_UI
	if (bench_iter <= 0) {
		IG_LOG(L_ERROR) << "No valid spp count given" << std::endl;
		return EXIT_FAILURE;
	}
	if (out_file == "") {
		IG_LOG(L_ERROR) << "No output file given" << std::endl;
		return EXIT_FAILURE;
	}
#endif

	std::unique_ptr<Runtime> runtime;
	try {
		RuntimeOptions opts;
		opts.DesiredTarget	   = target;
		opts.Device			   = device;
		opts.OverrideTechnique = overrideTechnique;
		opts.OverrideCamera	   = overrideCamera;

		runtime = std::make_unique<Runtime>(in_file, opts);
	} catch (const std::exception& e) {
		IG_LOG(L_ERROR) << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	const auto def		  = runtime->loadedRenderSettings();
	const int film_width  = a_film_width.value_or(def.FilmWidth);
	const int film_height = a_film_height.value_or(def.FilmHeight);
	const auto clip		  = trange.value_or(Vector2f(def.TMin, def.TMax));
	Camera camera(eye.value_or(def.CameraEye), dir.value_or(def.CameraDir), up.value_or(def.CameraUp),
				  fov.value_or(def.FOV), (float)film_width / (float)film_height,
				  clip(0), clip(1));
	runtime->setup(film_width, film_height);

#ifdef WITH_UI
	bool isDebug = runtime->configuration() & IG_C_RENDERER_DEBUG;
	if (!UI::init(film_width, film_height, runtime->getFramebuffer(), isDebug))
		return EXIT_FAILURE;

	DebugMode currentDebugMode = UI::currentDebugMode();
	runtime->setDebugMode(currentDebugMode);
#endif

	bool running	= true;
	bool done		= false;
	uint64_t timing = 0;
	uint32_t frames = 0;
	uint32_t iter	= 0;
	std::vector<double> samples_sec;
	while (!done) {
#ifdef WITH_UI
		bool prevRun = running;
		done		 = UI::handleInput(iter, running, camera);

		const DebugMode newDebugMode = UI::currentDebugMode();
		if (currentDebugMode != newDebugMode) {
			currentDebugMode = newDebugMode;
			runtime->setDebugMode(currentDebugMode);
			iter = 0;
		}
#endif

		if (running) {
			if (iter == 0)
				runtime->clearFramebuffer();

			auto ticks = std::chrono::high_resolution_clock::now();
			runtime->step(camera);
			iter++;
			auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ticks).count();

			if (bench_iter != 0) {
				samples_sec.emplace_back(1000.0 * double(SPP * film_width * film_height) / double(elapsed_ms));
				if (samples_sec.size() == bench_iter)
					break;
			}

			frames++;
			timing += elapsed_ms;
			if (frames > 10 || timing >= 2000) {
				auto frames_sec = double(frames) * 1000.0 / double(timing);
#ifdef WITH_UI
				std::ostringstream os;
				os << "Ignis [" << frames_sec << " FPS, "
				   << iter * SPP << " "
				   << "sample" << (iter * SPP > 1 ? "s" : "") << "]";
				UI::setTitle(os.str().c_str());
#endif
				frames = 0;
				timing = 0;
			}
		} else {
			frames++;

#ifdef WITH_UI
			if (prevRun != running || frames > 100) {
				std::ostringstream os;
				os << "Ignis [Paused, "
				   << iter * SPP << " "
				   << "sample" << (iter * SPP > 1 ? "s" : "") << "]";
				UI::setTitle(os.str().c_str());
				frames = 0;
				timing = 0;
			}
#endif
		}

#ifdef WITH_UI
		UI::update(iter);
#endif
	}

#ifdef WITH_UI
	UI::close();
#endif

	runtime.reset();

	if (bench_iter != 0) {
		auto inv = 1.0e-6;
		std::sort(samples_sec.begin(), samples_sec.end());
		IG_LOG(L_INFO) << "# " << samples_sec.front() * inv
					   << "/" << samples_sec[samples_sec.size() / 2] * inv
					   << "/" << samples_sec.back() * inv
					   << " (min/med/max Msamples/s)" << std::endl;
	}

	return EXIT_SUCCESS;
}
