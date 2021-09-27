#include "LoaderTechnique.h"
#include "Loader.h"
#include "Logger.h"
#include "serialization/VectorSerializer.h"

namespace IG {

bool LoaderTechnique::requireLights(LoaderContext& ctx)
{
	return ctx.TechniqueType == "path";
}

std::string LoaderTechnique::generate(LoaderContext& ctx, LoaderResult& result)
{
	std::stringstream stream;

	std::string tech_type = ctx.TechniqueType;

	if (tech_type == "ao") {
		stream << "  let technique = make_ao_renderer();";
	} else if (tech_type == "path") {
		int max_depth		 = 64;
		const auto technique = ctx.Scene.technique();
		if (technique)
			max_depth = technique->property("max_depth").getInteger(max_depth);

		stream << "  let technique = make_path_renderer(" << max_depth << ", num_lights, lights);";
	} else {
		stream << "  let technique = make_debug_renderer(settings.debug_mode);";
	}

	return stream.str();
}
} // namespace IG