#include "Camera.h"
#include "Interface.h"

#ifdef WITH_UI
#include "UI.h"
#endif

#include "Buffer.h"
#include "Color.h"
#include "Logger.h"
#include "bvh/BVH.h"
#include "math/BoundingBox.h"
#include "math/Triangle.h"

#include "generated_interface.h"

using namespace IG;

static inline void check_arg(int argc, char** argv, int arg, int n)
{
	if (arg + n >= argc)
		IG_LOG(L_ERROR) << "Option '" << argv[arg] << "' expects " << n << " arguments, got " << (argc - arg) << std::endl;
}

static inline void usage()
{
	std::cout << "Usage: rodent [options]\n"
			  << "Available options:\n"
			  << "   --help              Shows this message\n"
			  << "   --width  pixels     Sets the viewport horizontal dimension (in pixels)\n"
			  << "   --height pixels     Sets the viewport vertical dimension (in pixels)\n"
			  << "   --eye    x y z      Sets the position of the camera\n"
			  << "   --dir    x y z      Sets the direction vector of the camera\n"
			  << "   --up     x y z      Sets the up vector of the camera\n"
			  << "   --fov    degrees    Sets the horizontal field of view (in degrees)\n"
			  << "   --spp    spp        Enables benchmarking mode and sets the number of iterations based on the given spp\n"
			  << "   --bench  iterations Enables benchmarking mode and sets the number of iterations\n"
			  << "   -o       image.exr  Writes the output image to a file" << std::endl;
}

int main(int argc, char** argv)
{
	std::string out_file;
	size_t bench_iter = 0;

	// Get default settings from script
	InitialSettings default_settings = get_default_settings();
	default_settings.film_width		 = std::max(100, default_settings.film_width);
	default_settings.film_height	 = std::max(100, default_settings.film_height);

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "--width")) {
				check_arg(argc, argv, i, 1);
				default_settings.film_width = strtoul(argv[++i], nullptr, 10);
			} else if (!strcmp(argv[i], "--height")) {
				check_arg(argc, argv, i, 1);
				default_settings.film_height = strtoul(argv[++i], nullptr, 10);
			} else if (!strcmp(argv[i], "--eye")) {
				check_arg(argc, argv, i, 3);
				default_settings.eye.x = strtof(argv[++i], nullptr);
				default_settings.eye.y = strtof(argv[++i], nullptr);
				default_settings.eye.z = strtof(argv[++i], nullptr);
			} else if (!strcmp(argv[i], "--dir")) {
				check_arg(argc, argv, i, 3);
				default_settings.dir.x = strtof(argv[++i], nullptr);
				default_settings.dir.y = strtof(argv[++i], nullptr);
				default_settings.dir.z = strtof(argv[++i], nullptr);
			} else if (!strcmp(argv[i], "--up")) {
				check_arg(argc, argv, i, 3);
				default_settings.up.x = strtof(argv[++i], nullptr);
				default_settings.up.y = strtof(argv[++i], nullptr);
				default_settings.up.z = strtof(argv[++i], nullptr);
			} else if (!strcmp(argv[i], "--fov")) {
				check_arg(argc, argv, i, 1);
				default_settings.fov = strtof(argv[++i], nullptr);
			} else if (!strcmp(argv[i], "--spp")) {
				check_arg(argc, argv, i, 1);
				bench_iter = (size_t)std::ceil(strtoul(argv[++i], nullptr, 10) / (float)get_spp());
			} else if (!strcmp(argv[i], "--bench")) {
				check_arg(argc, argv, i, 1);
				bench_iter = strtoul(argv[++i], nullptr, 10);
			} else if (!strcmp(argv[i], "-o")) {
				check_arg(argc, argv, i, 1);
				out_file = argv[++i];
			} else if (!strcmp(argv[i], "--help")) {
				usage();
				return EXIT_SUCCESS;
			} else {
				IG_LOG(L_ERROR) << "Unknown option '" << argv[i] << "'" << std::endl;
				return EXIT_FAILURE;
			}
		} else {
			IG_LOG(L_ERROR) << "Unexpected argument '" << argv[i] << "'" << std::endl;
			return EXIT_FAILURE;
		}
	}

	Camera camera(
		Vector3f(default_settings.eye.x, default_settings.eye.y, default_settings.eye.z),
		Vector3f(default_settings.dir.x, default_settings.dir.y, default_settings.dir.z),
		Vector3f(default_settings.up.x, default_settings.up.y, default_settings.up.z),
		default_settings.fov,
		(float)default_settings.film_width / (float)default_settings.film_height);

#ifdef WITH_UI
	UI::init(default_settings.film_width, default_settings.film_height);
#endif

	setup_interface(default_settings.film_width, default_settings.film_height);

	// Force flush to zero mode for denormals
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
	_mm_setcsr(_mm_getcsr() | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
#endif

	auto spp		= get_spp();
	bool done		= false;
	uint64_t timing = 0;
	uint32_t frames = 0;
	uint32_t iter	= 0;
	std::vector<double> samples_sec;
	while (!done) {
#ifdef WITH_UI
		done = UI::handleInput(iter, camera);
#endif
		if (iter == 0)
			clear_pixels();

		Settings settings{
			Vec3{ camera.eye(0), camera.eye(1), camera.eye(2) },
			Vec3{ camera.dir(0), camera.dir(1), camera.dir(2) },
			Vec3{ camera.up(0), camera.up(1), camera.up(2) },
			Vec3{ camera.right(0), camera.right(1), camera.right(2) },
			camera.w,
			camera.h
		};

		auto ticks = std::chrono::high_resolution_clock::now();
		render(&settings, iter++);
		auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ticks).count();

		if (bench_iter != 0) {
			samples_sec.emplace_back(1000.0 * double(spp * default_settings.film_width * default_settings.film_height) / double(elapsed_ms));
			if (samples_sec.size() == bench_iter)
				break;
		}

		frames++;
		timing += elapsed_ms;
		if (frames > 10 || timing >= 2500) {
			auto frames_sec = double(frames) * 1000.0 / double(timing);
#ifdef WITH_UI
			std::ostringstream os;
			os << "Ignis [" << frames_sec << " FPS, "
			   << iter * spp << " "
			   << "sample" << (iter * spp > 1 ? "s" : "") << "]";
			UI::setTitle(os.str().c_str());
#endif
			frames = 0;
			timing = 0;
		}

#ifdef WITH_UI
		UI::update(iter);
#endif
	}

#ifdef WITH_UI
	UI::close();
#endif

	/*if (out_file != "") {
		save_image(out_file, width, height, iter);
		info("Image saved to '", out_file, "'");
	}*/

	cleanup_interface();

	if (bench_iter != 0) {
		auto inv = 1.0e-6;
		std::sort(samples_sec.begin(), samples_sec.end());
		IG_LOG(L_INFO) << "# " << samples_sec.front() * inv
					   << "/" << samples_sec[samples_sec.size() / 2] * inv
					   << "/" << samples_sec.back() * inv
					   << " (min/med/max Msamples/s)" << std::endl;
	}
	return 0;
}
