#include "LoaderBSDF.h"
#include "Logger.h"

#include "klems/KlemsLoader.h"

namespace IG {

constexpr float AIR_IOR	   = 1.000277f;
constexpr float GLASS_IOR  = 1.55f;
constexpr float RUBBER_IOR = 1.49f;

static void setup_microfacet(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	std::string alpha_u, alpha_v;
	if (bsdf->property("alpha_u").isValid()) {
		alpha_u = ctx.extractMaterialPropertyNumber(bsdf, "alpha_u", 0.1f, options.SurfaceParameter.c_str());
		alpha_v = ctx.extractMaterialPropertyNumber(bsdf, "alpha_v", 0.1f, options.SurfaceParameter.c_str());
	} else {
		alpha_u = ctx.extractMaterialPropertyNumber(bsdf, "alpha", 0.1f, options.SurfaceParameter.c_str());
		alpha_v = alpha_u;
	}

	os << "make_beckmann_distribution(math, " << options.SurfaceParameter << ", " << alpha_u << ", " << alpha_v << ")";
}

static void bsdf_error(const std::string& msg, std::ostream& os)
{
	IG_LOG(L_ERROR) << msg << std::endl;
	os << "make_black_bsdf()/* ERROR */";
}

static void bsdf_unknown(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext&, std::ostream& os)
{
	IG_LOG(L_WARNING) << "Unknown bsdf '" << bsdf->pluginType() << "'" << std::endl;
	os << "make_black_bsdf()/* Unknown " << bsdf->pluginType() << " */";
}

static void bsdf_diffuse(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_diffuse_bsdf(math, " << options.SurfaceParameter << ", " << ctx.extractMaterialPropertyColor(bsdf, "reflectance", 1.0f, options.SurfaceParameter.c_str()) << ")";
}

static void bsdf_orennayar(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_rough_diffuse_bsdf(math, " << options.SurfaceParameter << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "alpha", 0.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "reflectance", 1.0f, options.SurfaceParameter.c_str())
	   << ")";
}

static void bsdf_dielectric(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_glass_bsdf(math, " << options.SurfaceParameter << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "ext_ior", AIR_IOR, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "int_ior", GLASS_IOR, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_transmittance", 1.0f, options.SurfaceParameter.c_str()) << ")";
}

static void bsdf_thindielectric(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_thinglass_bsdf(math, " << options.SurfaceParameter << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "ext_ior", AIR_IOR, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "int_ior", GLASS_IOR, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_transmittance", 1.0f, options.SurfaceParameter.c_str()) << ")";
}

static void bsdf_mirror(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_mirror_bsdf(math, " << options.SurfaceParameter << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f, options.SurfaceParameter.c_str()) << ")";
}

static void bsdf_conductor(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_conductor_bsdf(math, " << options.SurfaceParameter << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "eta", 0.63660f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "k", 2.7834f, options.SurfaceParameter.c_str()) << ", " // TODO: Better defaults?
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f, options.SurfaceParameter.c_str()) << ")";
}

static void bsdf_rough_conductor(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_rough_conductor_bsdf(math, " << options.SurfaceParameter << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "eta", 0.63660f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "k", 2.7834f, options.SurfaceParameter.c_str()) << ", " // TODO: Better defaults?
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f, options.SurfaceParameter.c_str()) << ", ";
	setup_microfacet(bsdf, ctx, options, os);
	os << ")";
}

static void bsdf_plastic(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_plastic_bsdf(math, " << options.SurfaceParameter << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "ext_ior", AIR_IOR, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "int_ior", RUBBER_IOR, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "diffuse_reflectance", 0.5f, options.SurfaceParameter.c_str()) << ")";
}

static void bsdf_phong(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_phong_bsdf(math, " << options.SurfaceParameter << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "exponent", 30, options.SurfaceParameter.c_str()) << ")";
}

static void bsdf_disney(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_disney_bsdf(math, " << options.SurfaceParameter << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "base_color", 0.8f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "flatness", 0.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "metallic", 0.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "ior", GLASS_IOR, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "specular_tint", 0.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "roughness", 0.5f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "anisotropic", 0.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "sheen", 0.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "sheen_tint", 0.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "clearcoat", 0.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "clearcoat_gloss", 0.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "spec_trans", 0.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "relative_ior", 1.1f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "scatter_distance", 0.5f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "diff_trans", 0.0f, options.SurfaceParameter.c_str()) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "transmittance", 1.0f, options.SurfaceParameter.c_str()) << ")";
}

/*static void bsdf_prep_klems(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, std::ostream& os)
{
	const std::filesystem::path filename = ctx.handlePath(bsdf->property("filename").getString());
	const std::string id				 = LoaderContext::makeId(filename);
	const std::string out_file			 = "data/klems_" + filename.stem().generic_string() + ".bin";
	const bool ok						 = KlemsLoader::prepare(filename.generic_string(), out_file);

	os << "    let klems_" << id << " = device.load_klems(\"" << out_file.c_str() << "\");\n";
}

static void bsdf_klems(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	const std::filesystem::path filename = ctx.handlePath(bsdf->property("filename").getString());
	const std::string id				 = LoaderContext::makeId(filename);

	os << "make_klems_bsdf(math, surf, klems_" << id << ")";
}

static void bsdf_blend(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	const std::string first	 = bsdf->property("first").getString();
	const std::string second = bsdf->property("second").getString();

	if (first.empty() || second.empty()) {
		bsdf_error("Invalid blend bsdf", os);
	} else if (first == second) {
		os << LoaderBSDF::extract(ctx.Scene.bsdf(first), ctx);
	} else {
		os << LoaderBSDF::extract(ctx.Scene.bsdf(first), ctx) << ", "
		   << LoaderBSDF::extract(ctx.Scene.bsdf(second), ctx) << ", "
		   << ctx.extractMaterialPropertyNumber(bsdf, "weight", 0.5f, options.SurfaceParameter.c_str()) << ")";
	}
}

static void bsdf_mask(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	const std::string masked = bsdf->property("bsdf").getString();

	if (masked.empty())
		bsdf_error("Invalid mask bsdf", os);
	else
		os << "make_mix_bsdf(make_passthrough_bsdf(" << options.SurfaceParameter << "), "
		   << LoaderBSDF::extract(ctx.Scene.bsdf(masked), ctx) << ", "
		   << ctx.extractMaterialPropertyNumber(bsdf, "opacity", 0.5f) << ")";
}

static void bsdf_twosided(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	// Ignore
	const std::string other = bsdf->property("bsdf").getString();

	if (other.empty())
		bsdf_error("Invalid twosided bsdf", os);
	else
		os << LoaderBSDF::extract(ctx.Scene.bsdf(other), ctx);
}

static void bsdf_passthrough(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_passthrough_bsdf(surf)";
}

static void bsdf_null(const std::shared_ptr<Parser::Object>&, const LoaderContext&, const BSDFExtractOption& options, std::ostream& os)
{
	os << "make_black_bsdf()";
}

static void bsdf_normalmap(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	const std::string inner = bsdf->property("bsdf").getString();
	auto map				= ctx.extractMaterialPropertyColor(bsdf, "map", 1.0f, options.SurfaceParameter.c_str());

	BSDFExtractOption opts2 = options;
	opts2.SurfaceParameter += "a";

	if (inner.empty())
		bsdf_error("Invalid normalmap bsdf", os);
	else
		os << "make_normalmap(math, " << options.SurfaceParameter << ", @|" << opts2.SurfaceParameter << "| -> Bsdf { "
		   << LoaderBSDF::extract(ctx.Scene.bsdf(inner), ctx, opts2) << " }, "
		   << map << ")";
}

static void bsdf_bumpmap(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os)
{
	const std::string inner = bsdf->property("bsdf").getString();
	auto mapdx				= ctx.extractMaterialPropertyNumberDx(bsdf, "map", options.SurfaceParameter.c_str());
	auto mapdy				= ctx.extractMaterialPropertyNumberDy(bsdf, "map", options.SurfaceParameter.c_str());
	auto strength			= ctx.extractMaterialPropertyNumber(bsdf, "strength", 1.0f, options.SurfaceParameter.c_str());

	BSDFExtractOption opts2 = options;
	opts2.SurfaceParameter += "a";

	if (inner.empty())
		bsdf_error("Invalid normalmap bsdf", os);
	else
		os << "make_bumpmap(math, " << options.SurfaceParameter << ", @|" << opts2.SurfaceParameter << "| -> Bsdf { "
		   << LoaderBSDF::extract(ctx.Scene.bsdf(inner), ctx, opts2) << " }, "
		   << mapdx << ", "
		   << mapdy << ", "
		   << strength << ")";
}*/

using BSDFLoader = void (*)(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options, std::ostream& os);
static struct {
	const char* Name;
	BSDFLoader Loader;
} _generators[] = {
	{ "diffuse", bsdf_diffuse },
	{ "roughdiffuse", bsdf_orennayar },
	{ "glass", bsdf_dielectric },
	{ "dielectric", bsdf_dielectric },
	{ "roughdielectric", bsdf_dielectric }, /*TODO*/
	{ "thindielectric", bsdf_thindielectric },
	{ "mirror", bsdf_mirror }, // Specialized conductor
	{ "conductor", bsdf_conductor },
	{ "roughconductor", bsdf_rough_conductor },
	{ "phong", bsdf_phong },
	{ "disney", bsdf_disney },
	{ "plastic", bsdf_plastic },
	{ "roughplastic", bsdf_plastic }, /*TODO*/
	/*{ "klems", bsdf_klems },
	{ "blendbsdf", bsdf_blend },
	{ "mask", bsdf_mask },
	{ "twosided", bsdf_twosided },
	{ "passthrough", bsdf_passthrough },
	{ "null", bsdf_null },
	{ "bumpmap", bsdf_bumpmap },
	{ "normalmap", bsdf_normalmap },*/
	{ "", nullptr }
};

std::string LoaderBSDF::extract(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options)
{
	std::stringstream sstream;

	if (!bsdf) {
		bsdf_error("No bsdf given", sstream);
	} else {
		for (size_t i = 0; _generators[i].Loader; ++i) {
			if (_generators[i].Name == bsdf->pluginType()) {
				_generators[i].Loader(bsdf, ctx, options, sstream);
				return sstream.str();
			}
		}
		bsdf_unknown(bsdf, ctx, sstream);
	}

	return sstream.str();
}
} // namespace IG
