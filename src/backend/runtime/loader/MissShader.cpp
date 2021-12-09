#include "MissShader.h"
#include "Loader.h"
#include "LoaderLight.h"
#include "LoaderTechnique.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string MissShader::setup(LoaderContext& ctx)
{
	std::stringstream stream;

	stream << LoaderTechnique::generateHeader(ctx) << std::endl;

	stream << "#[export] fn ig_miss_shader(settings: &Settings, first: i32, last: i32) -> () {" << std::endl
		   << "  maybe_unused(settings);" << std::endl
		   << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
		   << std::endl;

	if (LoaderTechnique::requireLights(ctx))
		stream << LoaderLight::generate(ctx, true)
			   << std::endl;

	stream << "  let spp = " << ctx.SamplesPerIteration << " : i32;" << std::endl;

	// Will define technique
	stream << LoaderTechnique::generate(ctx, ctx.AOVCount) << std::endl
		   << std::endl;

	stream << "  device.handle_miss_shader(technique, first, last, spp)" << std::endl
		   << "}" << std::endl;

	return stream.str();
}

} // namespace IG