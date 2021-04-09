#include "Camera.h"
#include "Interface.h"

#include "Buffer.h"
#include "Color.h"
#include "Logger.h"
#include "bvh/BVH.h"
#include "math/BoundingBox.h"
#include "math/Triangle.h"

#include "generated_interface.h"

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
	std::cout << "Usage: ignis_trace [options]\n"
			  << "Available options:\n"
			  << "   --help           Shows this message\n"
			  << "   -n count         Samples per ray. Default is 1\n"
			  << "   -i list.txt      Read list of rays from file instead of the standard input\n"
			  << "   -o radiance.txt  Write radiance for each ray into file instead of standard output" << std::endl;
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
		stream >> ray.org.x >> ray.org.y >> ray.org.z >> ray.dir.x >> ray.dir.y >> ray.dir.z >> ray.tmin >> ray.tmax;

		if (ray.tmax <= ray.tmin)
			ray.tmax = std::numeric_limits<float>::max();

		// Normalize direction
		const float norm = std::sqrt(ray.dir.x * ray.dir.x + ray.dir.y * ray.dir.y + ray.dir.z * ray.dir.z);
		if (norm < std::numeric_limits<float>::epsilon()) {
			std::cerr << "Invalid ray given: Ray has zero direction!" << std::endl;
			continue;
		}
		ray.dir.x /= norm;
		ray.dir.y /= norm;
		ray.dir.z /= norm;

		// Calculate invert components
		ray.inv_dir.x = safe_rcp(ray.dir.x);
		ray.inv_dir.y = safe_rcp(ray.dir.y);
		ray.inv_dir.z = safe_rcp(ray.dir.z);

		ray.inv_org.x = -(ray.org.x * ray.inv_dir.x);
		ray.inv_org.y = -(ray.org.y * ray.inv_dir.y);
		ray.inv_org.z = -(ray.org.z * ray.inv_dir.z);

		rays.push_back(ray);

		std::cout << "R(O[" << ray.org.x << ", " << ray.org.y << ", " << ray.org.z << "], D[" << ray.dir.x << ", " << ray.dir.y << ", " << ray.dir.z << "], T[" << ray.tmin << ", " << ray.tmax << "])" << std::endl;
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
	uint32 sample_count = 1;
	std::string in_file;
	std::string out_file;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "-n")) {
				check_arg(argc, argv, i, 1);
				sample_count = strtoul(argv[++i], nullptr, 10);
			} else if (!strcmp(argv[i], "-o")) {
				check_arg(argc, argv, i, 1);
				out_file = argv[++i];
			} else if (!strcmp(argv[i], "-i")) {
				check_arg(argc, argv, i, 1);
				in_file = argv[++i];
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

	std::vector<Ray> rays;
	if (in_file.empty()) {
		rays = read_input(std::cin, false);
	} else {
		std::ifstream stream(in_file);
		rays = read_input(stream, true);
	}

	if (rays.empty()) {
		IG_LOG(L_ERROR) << "No rays given" << std::endl;
		return EXIT_FAILURE;
	}

	setup_interface(rays.size(), 1);

	Settings settings{
		Vec3{ 0, 0, 0 },
		Vec3{ 0, 0, 1 },
		Vec3{ 0, 1, 0 },
		Vec3{ 1, 0, 0 },
		1,
		1,
		(int)rays.size(),
		rays.data()
	};

	for (uint32 iter = 0; iter < sample_count; ++iter) {
		if (iter == 0)
			clear_pixels();

		render(&settings, iter++);
	}

	// Extract data
	auto film = get_pixels();
	if (out_file.empty()) {
		write_output(std::cout, false, film, rays.size(), sample_count);
	} else {
		std::ofstream stream(out_file);
		write_output(stream, true, film, rays.size(), sample_count);
	}

	cleanup_interface();

	return 0;
}
