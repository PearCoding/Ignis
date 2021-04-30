#include "Camera.h"

#ifdef WITH_UI
#include "UI.h"
#endif

#include "Loader.h"
#include "Logger.h"
#include "Runtime.h"

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
	std::string in_file;
	std::string out_file;
	size_t bench_iter = 0;
	int film_width	  = 800;
	int film_height	  = 600;
	Vector3f eye	  = Vector3f::Zero();
	Vector3f dir	  = Vector3f::UnitY();
	Vector3f up		  = Vector3f::UnitZ();
	float fov		  = 60;

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "--width")) {
				check_arg(argc, argv, i, 1);
				film_width = strtoul(argv[++i], nullptr, 10);
			} else if (!strcmp(argv[i], "--height")) {
				check_arg(argc, argv, i, 1);
				film_height = strtoul(argv[++i], nullptr, 10);
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
				bench_iter = (size_t)std::ceil(strtoul(argv[++i], nullptr, 10) / (float)SPP);
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
			if (in_file.empty()) {
				in_file = argv[i];
			} else {
				IG_LOG(L_ERROR) << "Unexpected argument '" << argv[i] << "'" << std::endl;
				return EXIT_FAILURE;
			}
		}
	}

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
		LoaderOptions opts;
		opts.target = Target::NVVM_STREAMING; // TODO
		opts.fusion = false;
		opts.device = 0;

		runtime = std::make_unique<Runtime>(in_file, opts);
	} catch (const std::exception& e) {
		IG_LOG(L_ERROR) << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	Camera camera(eye, dir, up, fov, (float)film_width / (float)film_height);
	runtime->setup(film_width, film_height);

#ifdef WITH_UI
	UI::init(film_width, film_height, runtime->getFramebuffer());
#endif

	bool done		= false;
	uint64_t timing = 0;
	uint32_t frames = 0;
	uint32_t iter	= 0;
	std::vector<double> samples_sec;
	while (!done) {
#ifdef WITH_UI
		done = UI::handleInput(iter, camera);
#endif

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
		if (frames > 10 || timing >= 2500) {
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

#ifdef WITH_UI
		UI::update(iter);
#endif
	}

#ifdef WITH_UI
	UI::close();
#endif

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
