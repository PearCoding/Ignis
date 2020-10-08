#include "UI.h"
#include "Interface.h"

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
	size_t width	  = 1080;
	size_t height	  = 720;
	float fov		  = 60.0f;
	Vector3f eye(0.0f, 0.0f, 0.0f), dir(0.0f, 0.0f, 1.0f), up(0.0f, 1.0f, 0.0f);

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "--width")) {
				check_arg(argc, argv, i, 1);
				width = strtoul(argv[++i], nullptr, 10);
			} else if (!strcmp(argv[i], "--height")) {
				check_arg(argc, argv, i, 1);
				height = strtoul(argv[++i], nullptr, 10);
			} else if (!strcmp(argv[i], "--eye")) {
				check_arg(argc, argv, i, 3);
				eye(0) = strtof(argv[++i], nullptr);
				eye(1) = strtof(argv[++i], nullptr);
				eye(2) = strtof(argv[++i], nullptr);
			} else if (!strcmp(argv[i], "--dir")) {
				check_arg(argc, argv, i, 3);
				dir(0) = strtof(argv[++i], nullptr);
				dir(1) = strtof(argv[++i], nullptr);
				dir(2) = strtof(argv[++i], nullptr);
			} else if (!strcmp(argv[i], "--up")) {
				check_arg(argc, argv, i, 3);
				up(0) = strtof(argv[++i], nullptr);
				up(1) = strtof(argv[++i], nullptr);
				up(2) = strtof(argv[++i], nullptr);
			} else if (!strcmp(argv[i], "--fov")) {
				check_arg(argc, argv, i, 1);
				fov = strtof(argv[++i], nullptr);
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

	Camera camera(eye, dir, up, fov, (float)width / (float)height);

#ifdef WITH_UI
	UI::init(width, height);
#endif

	setup_interface(width, height);

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
			samples_sec.emplace_back(1000.0 * double(spp * width * height) / double(elapsed_ms));
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
