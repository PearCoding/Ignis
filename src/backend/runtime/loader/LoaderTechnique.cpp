#include "LoaderTechnique.h"
#include "Loader.h"
#include "Logger.h"
#include "serialization/VectorSerializer.h"

namespace IG {

static void technique_empty_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
	stream << "static RayPayloadComponents = 0;" << std::endl
		   << "fn init_raypayload() = make_empty_payload();" << std::endl;
}

static void ao_body_loader(std::ostream& stream, size_t& aovs, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
	stream << "  let technique = make_ao_renderer();" << std::endl;
	aovs = 0;
}

static void debug_body_loader(std::ostream& stream, size_t& aovs, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
	stream << "  let technique = make_debug_renderer(settings.debug_mode);" << std::endl;
	aovs = 0;
}

static void path_body_loader(std::ostream& stream, size_t& aovs, const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext&)
{
	int max_depth = 64;
	max_depth	  = technique->property("max_depth").getInteger(max_depth);

	// stream << "  let aov_images = @|_id:i32| { make_empty_aov_image() };" << std::endl;
	// aovs = 0;
	stream << "  let aov_normals = device.load_aov_image(AOV_PATH_NORMAL, spp);" << std::endl;
	stream << "  let aov_di = device.load_aov_image(AOV_PATH_DIRECT, spp);" << std::endl;
	stream << "  let aov_di_weighted = device.load_aov_image(AOV_PATH_DIRECT_WEIGHTED, spp);" << std::endl;

	stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
		   << "    match(id) {" << std::endl;

	// TODO: We do not support constants in match (or any other useful location)!!!!!!!!!
	// This is completely unnecessary... we have to fix artic for that......
	stream << "      1 => aov_normals," << std::endl
		   << "      2 => aov_di," << std::endl
		   << "      3 => aov_di_weighted," << std::endl;

	stream << "      _ => make_empty_aov_image()" << std::endl
		   << "    }" << std::endl
		   << "  };" << std::endl;

	aovs = 3;

	stream << "  let technique = make_path_renderer(" << max_depth << ", num_lights, lights, aovs);" << std::endl;
}

static void path_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
	constexpr int C = 1 /* MIS */ + 3 /* Contrib */ + 1 /* Depth */;
	stream << "static RayPayloadComponents = " << C << ";" << std::endl
		   << "fn init_raypayload() = wrap_ptraypayload(PTRayPayload { mis = 0, contrib = white, depth = 1 });" << std::endl;
}

// Every body loader has to define 'technique' and 'aov_images'
using TechniqueBodyLoader = void (*)(std::ostream&, size_t&, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&);

// Every header loader has to define 'RayPayloadComponents' and 'init_raypayload()'
using TechniqueHeaderLoader = void (*)(std::ostream&, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&);

static struct TechniqueEntry {
	const char* Name;
	TechniqueBodyLoader BodyLoader;
	TechniqueHeaderLoader HeaderLoader;
} _generators[] = {
	{ "ao", ao_body_loader, technique_empty_header_loader },
	{ "path", path_body_loader, path_header_loader },
	{ "debug", debug_body_loader, technique_empty_header_loader },
	{ "", nullptr, nullptr }
};

static TechniqueEntry* getTechniqueEntry(const std::string& name)
{
	std::string tmp = name;
	std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
	for (size_t i = 0; _generators[i].HeaderLoader; ++i) {
		if (_generators[i].Name == tmp)
			return &_generators[i];
	}
	IG_LOG(L_ERROR) << "No technique type '" << name << "' available" << std::endl;
	return nullptr;
}

bool LoaderTechnique::requireLights(const LoaderContext& ctx)
{
	return ctx.TechniqueType == "path";
}

std::string LoaderTechnique::generate(const LoaderContext& ctx, size_t& aovs)
{
	const auto* entry = getTechniqueEntry(ctx.TechniqueType);
	if (!entry)
		return {};

	const auto technique = ctx.Scene.technique();

	std::stringstream stream;
	entry->BodyLoader(stream, aovs, ctx.TechniqueType, technique, ctx);

	return stream.str();
}

std::string LoaderTechnique::generateHeader(const LoaderContext& ctx, bool isRayGeneration)
{
	IG_UNUSED(isRayGeneration);

	const auto* entry = getTechniqueEntry(ctx.TechniqueType);
	if (!entry)
		return {};

	const auto technique = ctx.Scene.technique();

	std::stringstream stream;
	entry->HeaderLoader(stream, ctx.TechniqueType, technique, ctx);

	return stream.str();
}
} // namespace IG