#include "LoaderTechnique.h"
#include "Loader.h"
#include "Logger.h"
#include "serialization/VectorSerializer.h"

namespace IG {

size_t LoaderTechnique::getRayStateComponentCount(const LoaderContext& ctx)
{
	std::string tech_type = ctx.TechniqueType;

	if (tech_type == "ao") {
		return 0;
	} else if (tech_type == "path") {
		return 1 /* MIS */ + 3 /* Contrib */ + 1 /* Depth */;
	} else {
		return 0;
	}
}

bool LoaderTechnique::requireLights(const LoaderContext& ctx)
{
	return ctx.TechniqueType == "path";
}

std::string LoaderTechnique::generate(const LoaderContext& ctx)
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

std::string LoaderTechnique::generateCameraStateSetter(const LoaderContext& ctx)
{
	std::stringstream stream;

	std::string tech_type = ctx.TechniqueType;

	if (tech_type == "path")
		stream << "@|state: &RayState| -> () { state.set_1d(0, 0); state.set_3d(1, make_vec3(1,1,1)); state.set_1d(4, 0); }";
	else
		stream << "@|state: &RayState| -> () { }";

	return stream.str();
}
} // namespace IG