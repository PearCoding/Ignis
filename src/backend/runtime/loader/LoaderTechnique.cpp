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

static TechniqueInfo technique_empty_get_info(const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
	return {};
}

/////////////////////////

static void ao_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
	stream << "  let technique = make_ao_renderer();" << std::endl;
}

static void debug_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
	stream << "  let technique = make_debug_renderer(settings.debug_mode);" << std::endl;
}

static TechniqueInfo path_get_info(const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext&)
{
	TechniqueInfo info;
	if (technique->property("aov_normals").getBool(false))
		info.EnabledAOVs.push_back("Normals");

	if (technique->property("aov_mis").getBool(false)) {
		info.EnabledAOVs.push_back("Direct Weights");
		info.EnabledAOVs.push_back("NEE Weights");
		info.UseAdvancedShadowHandling = true;
	}

	if (technique->property("aov_stats").getBool(false)) {
		info.EnabledAOVs.push_back("Stats");
	}

	return info;
}

static void path_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext&)
{
	const int max_depth		= technique->property("max_depth").getInteger(64);
	const bool hasNormalAOV = technique->property("aov_normals").getBool(false);
	const bool hasMISAOV	= technique->property("aov_mis").getBool(false);
	const bool hasStatsAOV	= technique->property("aov_stats").getBool(false);

	// stream << "  let aov_images = @|_id:i32| { make_empty_aov_image() };" << std::endl;
	// aovs = 0;

	size_t counter = 1;
	if (hasNormalAOV)
		stream << "  let aov_normals = device.load_aov_image(" << counter++ << ", spp);" << std::endl;

	if (hasMISAOV) {
		stream << "  let aov_di = device.load_aov_image(" << counter++ << ", spp);" << std::endl
			   << "  let aov_nee = device.load_aov_image(" << counter++ << ", spp);" << std::endl;
	}

	if (hasStatsAOV)
		stream << "  let aov_stats = device.load_aov_image(" << counter++ << ", spp);" << std::endl;

	stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
		   << "    match(id) {" << std::endl;

	// TODO: We do not support constants in match (or any other useful location)!!!!!!!!!
	// This is completely unnecessary... we have to fix artic for that......
	if (hasNormalAOV)
		stream << "      1 => aov_normals," << std::endl;

	if (hasMISAOV) {
		stream << "      2 => aov_di," << std::endl
			   << "      3 => aov_nee," << std::endl;
	}

	if (hasStatsAOV)
		stream << "      4 => aov_stats," << std::endl;

	stream << "      _ => make_empty_aov_image()" << std::endl
		   << "    }" << std::endl
		   << "  };" << std::endl;

	stream << "  let technique = make_path_renderer(" << max_depth << ", num_lights, lights, aovs);" << std::endl;
}

static void path_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
	constexpr int C = 1 /* MIS */ + 3 /* Contrib */ + 1 /* Depth */;
	stream << "static RayPayloadComponents = " << C << ";" << std::endl
		   << "fn init_raypayload() = wrap_ptraypayload(PTRayPayload { mis = 0, contrib = white, depth = 1 });" << std::endl;
}

// Will return information about the enabled AOVs
using TechniqueGetInfo = TechniqueInfo (*)(const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&);

// Every body loader has to define 'technique'
using TechniqueBodyLoader = void (*)(std::ostream&, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&);

// Every header loader has to define 'RayPayloadComponents' and 'init_raypayload()'
using TechniqueHeaderLoader = void (*)(std::ostream&, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&);

static struct TechniqueEntry {
	const char* Name;
	TechniqueGetInfo GetInfo;
	TechniqueBodyLoader BodyLoader;
	TechniqueHeaderLoader HeaderLoader;
} _generators[] = {
	{ "ao", technique_empty_get_info, ao_body_loader, technique_empty_header_loader },
	{ "path", path_get_info, path_body_loader, path_header_loader },
	{ "debug", technique_empty_get_info, debug_body_loader, technique_empty_header_loader },
	{ "", nullptr, nullptr, nullptr }
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

TechniqueInfo LoaderTechnique::getInfo(const LoaderContext& ctx)
{
	const auto* entry = getTechniqueEntry(ctx.TechniqueType);
	if (!entry)
		return {};

	const auto technique = ctx.Scene.technique();

	return entry->GetInfo(ctx.TechniqueType, technique, ctx);
}

std::string LoaderTechnique::generate(const LoaderContext& ctx)
{
	const auto* entry = getTechniqueEntry(ctx.TechniqueType);
	if (!entry)
		return {};

	const auto technique = ctx.Scene.technique();

	std::stringstream stream;
	entry->BodyLoader(stream, ctx.TechniqueType, technique, ctx);

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