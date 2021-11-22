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

std::string LoaderTechnique::generateRayPayload(const LoaderContext& ctx, bool isRayGeneration)
{
	std::stringstream stream;

	std::string tech_type = ctx.TechniqueType;

	stream << "static RayPayloadComponents = " << getRayStateComponentCount(ctx) << ";" << std::endl;

	if (isRayGeneration) {
		if (tech_type == "path") {
			stream << "fn init_raypayload() = wrap_ptraypayload(PTRayPayload { mis = 0, contrib = white, depth = 1 });" << std::endl;
		} else {
			stream << "fn init_raypayload() = make_empty_payload();" << std::endl;
		}
	}

	return stream.str();
}
} // namespace IG