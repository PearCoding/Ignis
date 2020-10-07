#include <fstream>
#include <iostream>

#include "Generator.h"
#include "Logger.h"
#include "Target.h"

#include "HW_Features.h"

using namespace IG;

static inline Target getRecommendedTarget()
{
#if defined(IG_HAS_HW_FEATURE_AVX2)
	return Target::AVX2;
#elif defined(IG_HAS_HW_FEATURE_AVX)
	return Target::AVX;
#elif defined(IG_HAS_HW_FEATURE_SSE4_2)
	return Target::SSE42;
#elif defined(IG_HAS_HW_FEATURE_SSE2)
	return Target::ASIMD;
#else
	return Target::GENERIC;
#endif
}

static void usage()
{
	std::cout << "iggenerator [options] file\n"
			  << "Available options:\n"
			  << "    -h     --help                Shows this message\n"
			  << "    -o     --output              Sets the output file (default: main.art)\n"
			  << "    -t     --target              Sets the target platform (default: autodetect CPU)\n"
			  << "    -d     --device              Sets the device to use on the selected platform (default: 0)\n"
			  << "           --max-path-len        Sets the maximum path length (default: 64)\n"
			  << "    -spp   --samples-per-pixel   Sets the number of samples per pixel (default: 4)\n"
			  << "           --fusion              Enables megakernel shader fusion (default: disabled)\n"
			  << "Available targets:\n"
			  << "    generic, sse42, avx, avx2, asimd,\n"
			  << "    nvvm = nvvm-streaming, nvvm-megakernel,\n"
			  << "    amdgpu = amdgpu-streaming, amdgpu-megakernel\n"
			  << std::flush;
}

static bool check_option(int i, int argc, char** argv)
{
	if (i + 1 >= argc) {
		IG_LOG(L_ERROR) << "Missing argument for '" << argv[i] << "'. Aborting." << std::endl;
		return false;
	}
	return true;
}

int main(int argc, char** argv)
{
	if (argc <= 1) {
		// No logger use, as this may happen very often and the actual application did not start yet
		std::cerr << "Not enough arguments. Run with --help to get a list of options." << std::endl;
		return EXIT_FAILURE;
	}

	std::string filename;
	std::string output;
	GeneratorOptions options;
	options.device			= 0;
	options.spp				= 4;
	options.max_path_length = 64;
	options.target			= Target::INVALID;
	options.fusion			= false;

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
				usage();
				return EXIT_SUCCESS;
			} else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
				if (!check_option(i++, argc, argv))
					return EXIT_FAILURE;
				if (output != "") {
					IG_LOG(L_ERROR) << "Only one output filename can be set. Aborting." << std::endl;
					return EXIT_FAILURE;
				}
				output = argv[i];
			} else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--target")) {
				if (!check_option(i++, argc, argv))
					return EXIT_FAILURE;
				if (!strcmp(argv[i], "sse42"))
					options.target = Target::SSE42;
				else if (!strcmp(argv[i], "avx"))
					options.target = Target::AVX;
				else if (!strcmp(argv[i], "avx2"))
					options.target = Target::AVX2;
				else if (!strcmp(argv[i], "asimd"))
					options.target = Target::ASIMD;
				else if (!strcmp(argv[i], "nvvm") || !strcmp(argv[i], "nvvm-streaming"))
					options.target = Target::NVVM_STREAMING;
				else if (!strcmp(argv[i], "nvvm-megakernel"))
					options.target = Target::NVVM_MEGAKERNEL;
				else if (!strcmp(argv[i], "amdgpu") || !strcmp(argv[i], "amdgpu-streaming"))
					options.target = Target::AMDGPU_STREAMING;
				else if (!strcmp(argv[i], "amdgpu-megakernel"))
					options.target = Target::AMDGPU_MEGAKERNEL;
				else if (!strcmp(argv[i], "generic"))
					options.target = Target::GENERIC;
				else {
					IG_LOG(L_ERROR) << "Unknown target '" << argv[i] << "'. Aborting." << std::endl;
					return EXIT_FAILURE;
				}
			} else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--device")) {
				if (!check_option(i++, argc, argv))
					return EXIT_FAILURE;
				options.device = strtoul(argv[i], NULL, 10);
			} else if (!strcmp(argv[i], "-spp") || !strcmp(argv[i], "--samples-per-pixel")) {
				if (!check_option(i++, argc, argv))
					return EXIT_FAILURE;
				options.spp = strtol(argv[i], NULL, 10);
			} else if (!strcmp(argv[i], "--max-path-len")) {
				if (!check_option(i++, argc, argv))
					return EXIT_FAILURE;
				options.max_path_length = strtol(argv[i], NULL, 10);
			} else if (!strcmp(argv[i], "--fusion")) {
				options.fusion = true;
			} else {
				IG_LOG(L_ERROR) << "Unknown option '" << argv[i] << "'. Aborting." << std::endl;
				return EXIT_FAILURE;
			}
		} else {
			if (filename != "") {
				IG_LOG(L_ERROR) << "Only one file can be converted. Aborting." << std::endl;
				return EXIT_FAILURE;
			}
			filename = argv[i];
		}
	}

	if (options.fusion && options.target != Target::NVVM_MEGAKERNEL && options.target != Target::AMDGPU_MEGAKERNEL) {
		IG_LOG(L_ERROR) << "Fusion is only available for megakernel targets. Aborting." << std::endl;
		return EXIT_FAILURE;
	}

	if (filename == "") {
		IG_LOG(L_ERROR) << "Please specify an input file to convert. Aborting." << std::endl;
		return EXIT_FAILURE;
	}

	if (options.target == Target::INVALID) {
		options.target = getRecommendedTarget();
		if (options.target == Target::GENERIC)
			IG_LOG(L_WARNING) << "No vector instruction set detected. Select the target platform manually to improve performance." << std::endl;
	}

	if (output == "")
		output = "main.art";

	std::ofstream of(output);
	if (generate(filename, options, of))
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}
