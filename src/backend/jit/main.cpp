#include <artic/emit.h>
#include <artic/locator.h>
#include <artic/log.h>
#include <artic/print.h>

#include <thorin/be/codegen.h>
#include <thorin/world.h>

#include <fstream>
#include <string>
#include <vector>

extern const char* ig_api[];
extern const char* ig_api_paths[];

#ifdef IG_DEBUG
constexpr int OPT_LEVEL = 0;
constexpr bool DEBUG	= true;
#else
constexpr int OPT_LEVEL = 3;
constexpr bool DEBUG	= false;
#endif

int main(int argc, char** argv)
{
	artic::Locator locator;
	artic::Log log(artic::log::err, &locator);
	log.max_errors = -1;

	std::vector<std::string> paths;
	std::vector<std::string> file_data;
	for (int i = 0; ig_api[i]; ++i) {
		paths.emplace_back(std::string("[EMBEDDED]") + ig_api_paths[i]);
		file_data.emplace_back(ig_api[i]);
	}

	thorin::World world("__internal_interface");
	world.set(std::make_shared<thorin::Stream>(std::cerr));

	artic::ast::ModDecl program;
	bool success = artic::compile(
		paths, file_data,
		false,
		true,
		program, world, log);

	log.print_summary();

	if (!success)
		return EXIT_FAILURE;

	world.cleanup();
	world.opt();

	thorin::DeviceBackends backends(world, OPT_LEVEL, DEBUG);
	auto emit_to_file = [&](thorin::CodeGen& cg) {
		auto name = std::string("TEST") + cg.file_ext();
		std::ofstream file(name);
		if (!file)
			artic::log::error("cannot open '{}' for writing", name);
		else
			cg.emit_stream(file);
	};
	for (auto& cg : backends.cgs) {
		if (cg)
			emit_to_file(*cg);
	}

	return EXIT_SUCCESS;
}