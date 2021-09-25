#include "LoaderTechnique.h"
#include "Loader.h"
#include "Logger.h"
#include "serialization/VectorSerializer.h"

namespace IG {
std::string LoaderTechnique::generate(bool is_miss_shader, LoaderContext& ctx, LoaderResult& result)
{
	std::stringstream stream;

	std::string tech_type = ctx.TechniqueType;

	// TODO: Setup path tracer
	// and also setup lights if necessary 
	// (it is already defined if its a hit shader)

	if (tech_type == "ao") {
		stream << "make_ao_renderer()";
	} /* else if (tech_type == "path") {
		const auto technique = lopts.Scene.technique();
		if (technique) {
			settings.MaxPathLength = technique->property("max_depth").getInteger(64);
		}
	} */
	else {
		stream << "make_debug_renderer(settings.debug_mode)";
	}

	return stream.str();
}
} // namespace IG