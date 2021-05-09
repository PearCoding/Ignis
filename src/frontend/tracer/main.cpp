#include "Logger.h"
#include "Runtime.h"

#include <fstream>
#include <sstream>

using namespace IG;

static inline void check_arg(int argc, char** argv, int arg, int n)
{
	if (arg + n >= argc)
		IG_LOG(L_ERROR) << "Option '" << argv[arg] << "' expects " << n << " arguments, got " << (argc - arg) << std::endl;
}

static inline void usage()
{
	std::cout << "Usage: igtrace file [options]\n"
			  << "Available options:\n"
			  << "   -h      --help                   Shows this message\n"
			  << "   -t      --target   target        Sets the target platform (default: autodetect CPU)\n"
			  << "   -d      --device   device        Sets the device to use on the selected platform (default: 0)\n"
			  << "           --cpu                    Use autodetected CPU target\n"
			  << "           --gpu                    Use autodetected GPU target\n"
			  << "   -n      --count    count         Samples per ray. Default is 1\n"
			  << "   -i      --input    list.txt      Read list of rays from file instead of the standard input\n"
			  << "   -o      --output   radiance.txt  Write radiance for each ray into file instead of standard output" << std::endl;
}

static inline float safe_rcp(float x)
{
	constexpr float min_rcp = 1e-8f;
	if ((x > 0 ? x : -x) < min_rcp) {
		return std::signbit(x) ? -std::numeric_limits<float>::max() : std::numeric_limits<float>::max();
	} else {
		return 1 / x;
	}
}

static std::vector<Ray> read_input(std::istream& is, bool file)
{
	std::vector<Ray> rays;
	for (std::string line; std::getline(is, line);) {
		if (line.empty())
			break;

		std::stringstream stream(line);

		Ray ray;
		stream >> ray.Origin(0) >> ray.Origin(1) >> ray.Origin(2) >> ray.Direction(0) >> ray.Direction(1) >> ray.Direction(2) >> ray.Range(0) >> ray.Range(1);

		if (ray.Range(1) <= ray.Range(0))
			ray.Range(1) = std::numeric_limits<float>::max();

		rays.push_back(ray);
	}

	return rays;
}

static void write_output(std::ostream& is, bool file, float* data, size_t count, uint32 spp)
{
	for (size_t i = 0; i < count; ++i) {
		is << data[3 * i + 0] / spp << " " << data[3 * i + 1] / spp << " " << data[3 * i + 2] / spp << std::endl;
	}
}

int main(int argc, char** argv)
{
	std::string scene_file;
	uint32 sample_count = 1;
	std::string ray_file;
	std::string out_file;
	Target target = Target::INVALID;
	int device	  = 0;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "-n") || !strcmp(argv[i], "--count")) {
				check_arg(argc, argv, i, 1);
				sample_count = strtoul(argv[++i], nullptr, 10);
			} else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
				check_arg(argc, argv, i, 1);
				out_file = argv[++i];
			} else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--input")) {
				check_arg(argc, argv, i, 1);
				ray_file = argv[++i];
			} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
				usage();
				return EXIT_SUCCESS;
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
			} else {
				IG_LOG(L_ERROR) << "Unknown option '" << argv[i] << "'" << std::endl;
				return EXIT_FAILURE;
			}
		} else {
			if (scene_file.empty()) {
				scene_file = argv[i];
			} else {
				IG_LOG(L_ERROR) << "Unexpected argument '" << argv[i] << "'" << std::endl;
				return EXIT_FAILURE;
			}
		}
	}

	if (target == Target::INVALID)
		target = getRecommendedCPUTarget();

	std::vector<Ray> rays;
	if (ray_file.empty()) {
		rays = read_input(std::cin, false);
	} else {
		std::ifstream stream(ray_file);
		rays = read_input(stream, true);
	}

	if (rays.empty()) {
		IG_LOG(L_ERROR) << "No rays given" << std::endl;
		return EXIT_FAILURE;
	}

	std::unique_ptr<Runtime> runtime;
	try {
		RuntimeOptions opts;
		opts.DesiredTarget	  = target;
		opts.Device			  = device;
		opts.OverrideCamera	  = "list";
		opts.OverrideFilmSize = std::make_pair<uint32, uint32>(rays.size(), 1);

		runtime = std::make_unique<Runtime>(scene_file, opts);
	} catch (const std::exception& e) {
		IG_LOG(L_ERROR) << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	runtime->setup(rays.size(), 1);

	std::vector<float> accum_data;
	std::vector<float> iter_data;
	for (uint32 iter = 0; iter < sample_count; ++iter) {
		runtime->trace(rays, iter_data);

		if (accum_data.size() != iter_data.size())
			accum_data.resize(iter_data.size(), 0.0f);
		for (size_t i = 0; i < iter_data.size(); ++i)
			accum_data[i] += iter_data[i];
	}

	// Extract data
	if (out_file.empty()) {
		write_output(std::cout, false, accum_data.data(), rays.size(), sample_count);
	} else {
		std::ofstream stream(out_file);
		write_output(stream, true, accum_data.data(), rays.size(), sample_count);
	}

	return EXIT_SUCCESS;
}
