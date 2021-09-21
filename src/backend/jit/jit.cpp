#include <anydsl_jit.h>

#include <sstream>
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

// TODO: Change this to a library and adapt it for the several programming stages (emitter, shader, [traversal])
bool ig_compile_source(const std::string& src)
{
	std::stringstream source;

	for (int i = 0; ig_api[i]; ++i)
		source << ig_api[i];
	
	source << std::endl;
	source << src;

	std::string source_str = source.str();
	return anydsl_compile(source_str.c_str(), source_str.size(), OPT_LEVEL) >= 0;
}