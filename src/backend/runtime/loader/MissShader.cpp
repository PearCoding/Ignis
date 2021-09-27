#include "MissShader.h"
#include "Loader.h"
#include "LoaderLight.h"
#include "LoaderTechnique.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string MissShader::setup(LoaderContext& ctx, LoaderResult& result)
{
	std::stringstream stream;

	stream << "#[export] fn ig_miss_shader(settings: &Settings, first: i32, last: i32) -> () {" << std::endl;
	stream << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl;
	stream << std::endl;

	if (LoaderTechnique::requireLights(ctx))
		stream << LoaderLight::generate(ctx, result);

	stream << LoaderTechnique::generate(ctx, result) << std::endl;
	stream << std::endl;

	stream << "  let spp = 4 : i32;" << std::endl; // TODO ?
	stream << "  device.handle_miss_shader(technique, first, last, spp)" << std::endl;
	stream << "}" << std::endl;

	return stream.str();
}

} // namespace IG