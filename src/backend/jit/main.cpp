#include <artic/emit.h>
#include <artic/locator.h>
#include <artic/log.h>
#include <artic/print.h>

#include <thorin/be/codegen.h>
#include <thorin/world.h>

#include <string>
#include <vector>

extern const char* ig_api[];

int main(int argc, char** argv)
{
	artic::Locator locator;
	artic::Log log(artic::log::err, &locator);
	log.max_errors = -1;

	std::vector<std::string> paths;
	std::vector<std::string> file_data;
	for (int i = 0; ig_api[i]; ++i) {
		paths.emplace_back("...");
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

	return 0;
}