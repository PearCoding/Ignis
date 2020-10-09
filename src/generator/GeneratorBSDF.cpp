#include "GeneratorBSDF.h"
#include "Logger.h"

namespace IG {
static void bsdf_error(const std::string& msg, std::ostream& os)
{
	IG_LOG(L_ERROR) << msg << std::endl;
	os << "make_black_bsdf()/* ERROR */";
}

static void bsdf_diffuse(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext& ctx, std::ostream& os)
{
	os << "make_diffuse_bsdf(math, surf, " << ctx.extractMaterialPropertyColor(bsdf, "reflectance") << ")";
}

static void bsdf_orennayar(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext& ctx, std::ostream& os)
{
	os << "make_rough_diffuse_bsdf(math, surf, "
	   << ctx.extractMaterialPropertyNumber(bsdf, "alpha", 0.0f) << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "reflectance")
	   << ")";
}

static void bsdf_dielectric(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext& ctx, std::ostream& os)
{
	os << "make_glass_bsdf(math, surf, "
	   << ctx.extractMaterialPropertyNumber(bsdf, "ext_ior", 1.000277f) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "int_ior", 1.5046f) << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f) << ", "
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_transmittance", 1.0f) << ")";
}

static void bsdf_conductor(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext& ctx, std::ostream& os)
{
	os << "make_conductor_bsdf(math, surf, "
	   << ctx.extractMaterialPropertyNumber(bsdf, "eta", 0.63660f) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "k", 2.7834f) << ", " // TODO: Better defaults?
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f) << ")";
}

static void bsdf_phong(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext& ctx, std::ostream& os)
{
	os << "make_phong_bsdf(math, surf, "
	   << ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f) << ", "
	   << ctx.extractMaterialPropertyNumber(bsdf, "exponent", 30) << ")";
}

static void bsdf_blend(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext& ctx, std::ostream& os)
{
	if (bsdf->anonymousChildren().size() != 2) {
		bsdf_error("Invalid blend bsdf", os);
	} else {
		os << GeneratorBSDF::extract(bsdf->anonymousChildren()[0], ctx) << ", "
		   << GeneratorBSDF::extract(bsdf->anonymousChildren()[1], ctx) << ", "
		   << ctx.extractMaterialPropertyNumber(bsdf, "weight", 0.5f) << ")";
	}
}

static void bsdf_mask(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext& ctx, std::ostream& os)
{
	if (bsdf->anonymousChildren().size() != 1) {
		bsdf_error("Invalid mask bsdf", os);
	} else {
		os << "make_mix_bsdf(make_passthrough_bsdf(surf), "
		   << GeneratorBSDF::extract(bsdf->anonymousChildren()[0], ctx) << ", "
		   << ctx.extractMaterialPropertyNumber(bsdf, "opacity", 0.5f) << ")";
	}
}

static void bsdf_twosided(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext& ctx, std::ostream& os)
{ /* Ignore */
	if (bsdf->anonymousChildren().size() != 1)
		bsdf_error("Invalid twosided bsdf", os);
	else
		os << GeneratorBSDF::extract(bsdf->anonymousChildren()[0], ctx);
}

static void bsdf_null(const std::shared_ptr<TPMObject>&, const GeneratorContext&, std::ostream& os)
{
	os << "make_black_bsdf()/* Null */";
}

static void bsdf_unknown(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext&, std::ostream& os)
{
	IG_LOG(L_WARNING) << "Unknown bsdf '" << bsdf->pluginType() << "'" << std::endl;
	os << "make_black_bsdf()/* Null */";
}

using BSDFGenerator = void (*)(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext& ctx, std::ostream& os);
static struct {
	const char* Name;
	BSDFGenerator Generator;
} _generators[] = {
	{ "diffuse", bsdf_diffuse },
	{ "roughdiffuse", bsdf_orennayar },
	{ "dielectric", bsdf_dielectric },
	{ "roughdielectric", bsdf_dielectric }, /*TODO*/
	{ "thindielectric", bsdf_dielectric },	/*TODO*/
	{ "conductor", bsdf_conductor },
	{ "roughconductor", bsdf_conductor }, /*TODO*/
	{ "phong", bsdf_phong },
	{ "plastic", bsdf_phong },		/*TODO*/
	{ "roughplastic", bsdf_phong }, /*TODO*/
	{ "blendbsdf", bsdf_blend },
	{ "mask", bsdf_mask },
	{ "twosided", bsdf_twosided },
	{ "null", bsdf_null },
	{ "", nullptr }
};

std::string GeneratorBSDF::extract(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext& ctx)
{
	std::stringstream sstream;

	if (!bsdf) {
		bsdf_error("No bsdf given", sstream);
	} else {
		for (size_t i = 0; _generators[i].Generator; ++i) {
			if (_generators[i].Name == bsdf->pluginType()) {
				_generators[i].Generator(bsdf, ctx, sstream);
				return sstream.str();
			}
		}
		bsdf_unknown(bsdf, ctx, sstream);
	}

	return sstream.str();
}
} // namespace IG