#include "MissShader.h"
#include "Loader.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string MissShader::setup(LoaderContext& ctx, LoaderResult& result)
{
	std::stringstream stream;

	stream << "#[export] fn ig_main(settings: &Settings, iter: i32, first: i32, last: i32) -> () {" << std::endl;
	stream << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl;
	stream << std::endl;

	stream << "  let technique = make_ao_renderer()(iter);" << std::endl;
	stream << std::endl;

	stream << "  let spp = 4 : i32;" << std::endl; // TODO ?
	stream << "  device.handle_miss_shader(technique, first, last, spp)" << std::endl;
	stream << "}" << std::endl;

	return stream.str();
}

} // namespace IG